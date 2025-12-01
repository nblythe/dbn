/**
 * @file dbn.c
 * @brief Databento live market data client
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for example usage.
 */

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sodium.h>
#include <liburing.h>

#include "dbn.h"


/**
 * @brief Receive a Databento control message from a socket.
 *
 * @param sock Socket file descriptor
 *
 * @return Pointer to ASCII string received from the socket, or NULL on error. Caller must free.
 *
 * Control messages are only received during the initial setup phase
 * of a Databento connection, so this code does not need to be performant.
 */
static char *receive_control_message(int sock)
{
  char *msg = malloc(64);
  ssize_t c = 64;
  ssize_t n = 0;

  while (true)
  {
    if (n > c / 2)
    {
      c *= 2;
      msg = realloc(msg, c);
    }

    ssize_t m = recv(sock, msg + n, 1, 0);
    if (m > 0)
    {
      n += m;
      if (msg[n - 1] == '\n')
      {
        msg[n - 1] = 0;
        return msg;
      }
    }
    else
    {
      free(msg);
      return NULL;
    }
  }
}


/**
 * @brief Get a field value from a received Databento control message, by key.
 *
 * @param msg Pointer to null-terminated control message.
 * @param key Pointer to null-terminated field key.
 *
 * @return Pointer to field value corresponding to the key, or NULL if no matching key. Caller must free.
 */
static char *get_control_message_field(char *msg, char *key)
{
  int n = strlen(msg);
  char *start = strstr(msg, key);
  if (!start) return NULL;

  start += strlen(key);
  if (start < msg + n - 1)
  {
    start++;
    char *end = strchr(start, '|');
    if (end == NULL) end = msg + n;

    int m = end - start;
    char *value = malloc(m + 1);
    memcpy(value, start, m);
    value[m] = 0;
    return value;
  }
  else return NULL;
}


/**
 * @brief Invoke an error handler, if not NULL.
 *
 * @param dbn Pointer to client object.
 * @param fatal Indicates if the error being reported is fatal or not.
 * @param format printf-style format string.
 * @param  ... Arguments for format string.
 */
static void invoke_error_handler(
  dbn_t *dbn,
  bool fatal,
  const char *format,
  ...)
{
  if (!dbn->on_error) return;

  va_list args;
  va_start(args, format);

  int size = 1 + vsnprintf(NULL, 0, format, args);
  va_end(args);

  char *msg = malloc(size);
  va_start(args, format);
  vsnprintf(msg, size, format, args);
  va_end(args);

  dbn->on_error(
    dbn,
    fatal,
    msg);

  free(msg);
}


void dbn_init(
  dbn_t *dbn,
  dbn_on_error_t on_error,
  dbn_on_msg_t on_msg,
  void *ctx)
{
  memset(dbn, 0, sizeof(dbn_t));
  dbn->on_error = on_error;
  dbn->on_msg = on_msg;
  dbn->ctx = ctx;
}


int dbn_connect(
  dbn_t *dbn,
  const char *api_key,
  const char *dataset,
  bool ts_out)
{
  /*
   * Create socket.
   */
  if ((dbn->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Failed to create socket (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return false;
  }


  /*
   * Set a 64 MiB socket buffer.
   */
  int buffer_size = 1024 * 1024 * 64;
  if (setsockopt(
    dbn->sock,
    SOL_SOCKET,
    SO_RCVBUF,
    &buffer_size,
    sizeof(int)) < 0)
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Failed to set socket buffer size (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return -1;
  }


  /*
   * Actual buffer size could end up bigger. Whatever it is,
   * make our buffer size the same.
   */
  socklen_t optlen = sizeof(int);
  getsockopt(
    dbn->sock,
    SOL_SOCKET,
    SO_RCVBUF,
    &buffer_size,
    &optlen);

  if (buffer_size < 1024 * 1024 * 64)
  {
    invoke_error_handler(
      dbn,
      true,
      "Failed to set socket buffer size (size is %d)",
      buffer_size);
    errno = ENOMEM;
    return -1;
  }

  dbn->capacity = buffer_size;


  /*
   * Allocate two buffers for liburing plus one buffer for "leftover" data
   * that might happen when TCP read timing misaligns with internal kernel
   * buffering of Databento TCP packets (which themselves always align with
   * messages).
   */
  dbn->buffer0 = malloc(dbn->capacity);
  dbn->buffer1 = malloc(dbn->capacity);
  dbn->leftover = malloc(dbn->capacity);
  if (!dbn->buffer0 || !dbn->buffer1 || !dbn->leftover)
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Failed to allocate buffer (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return -1;
  }


  /*
   * Initialize the io_uring. Won't be used until we finish all early
   * comms and are ready to receive dbn-encoded messages.
   */
  io_uring_queue_init(2, &dbn->ring, 0);


  /*
   * Build the API FQDN.
   */
  int dataset_length = 1 + strlen(dataset);
  char *adjusted_dataset = malloc(dataset_length);
  memcpy(adjusted_dataset, dataset, dataset_length);
  for (int i = 0; i < dataset_length; i++)
  {
    if (adjusted_dataset[i] == '.') adjusted_dataset[i] = '-';
  }

  int fqdn_length = 1 + snprintf(NULL, 0, "%s.lsg.databento.com", adjusted_dataset);
  char *fqdn = malloc(fqdn_length);
  snprintf(fqdn, fqdn_length, "%s.lsg.databento.com", adjusted_dataset);

  free(adjusted_dataset);


  /*
   * Resolve the API FQDN.
   */
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(13000);

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = 0;
  int r;
  if ((r = getaddrinfo(fqdn, NULL, &hints, &res)))
  {
    invoke_error_handler(
      dbn,
      true,
      "Failed to resolve %s (%s)",
      fqdn,
      gai_strerror(r));
    free(fqdn);
    errno = ENXIO;
    return -1;
  }

  addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;

  freeaddrinfo(res);
  free(fqdn);


  /*
   * Connect.
   */
  char ipstr[INET_ADDRSTRLEN];
  inet_ntop(addr.sin_family, &addr.sin_addr, ipstr, sizeof(ipstr));

  if (connect(dbn->sock, (struct sockaddr *)&addr, sizeof(addr)))
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Failed to connect (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return -1;
  }


  /*
   * Receive lsg_version message.
   */
  char *msg0 = receive_control_message(dbn->sock);

  if (!msg0)
  {
    invoke_error_handler(
      dbn,
      true,
      "Error receiving first control message");
    errno = EBADMSG;
    return -1;
  }

  char *lsg_version = get_control_message_field(msg0, "lsg_version");
  free(msg0);
  if (!lsg_version)
  {
    invoke_error_handler(
      dbn,
      true,
      "First control message is missing lsg_version field");
    errno = EBADMSG;
    return -1;
  }

  free(lsg_version);


  /*
   * Receive cram message.
   */
  char *msg1 = receive_control_message(dbn->sock);

  if (!msg1)
  {
    invoke_error_handler(
      dbn,
      true,
      "Error receiving second control message");
    errno = EBADMSG;
    return -1;
  }

  char *cram = get_control_message_field(msg1, "cram");
  free(msg1);

  if (!cram)
  {
    invoke_error_handler(
      dbn,
      true,
      "Second control message is missing cram field");
    errno = EBADMSG;
    return -1;
  }


  /*
   * Compute and send auth message.
   */
  if (sodium_init() < 0)
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Failed to initialize libsodium (errno %d: %s)",
      e,
      strerror(e));
    free(cram);
    errno = e;
    return -1;
  }

  int len_ccram = snprintf(NULL, 0, "%s|%s", cram, api_key);
  char *ccram = malloc(len_ccram + 1);
  memset(ccram, 0, len_ccram + 1);
  snprintf(ccram, len_ccram + 1, "%s|%s", cram, api_key);

  free(cram);

  unsigned char hash[32];
  crypto_hash_sha256(hash, (const unsigned char *)ccram, strlen(ccram));

  free(ccram);

  char hex[65];
  for (int i = 0; i < 32; i++)
  {
    snprintf(hex + i * 2, 3, "%02x", hash[i]);
  }
  hex[64] = 0;

  char bucket_id[6];
  strcpy(bucket_id, api_key + strlen(api_key) - 5);

  int len_auth = snprintf(
    NULL, 0,
    "auth=%s-%s|dataset=%s|encoding=dbn|ts_out=%d\n",
    hex,
    bucket_id,
    dataset,
    ts_out ? 1 : 0);

  char *auth = malloc(len_auth + 1);
  memset(auth, 0, len_auth + 1);

  snprintf(
    auth, len_auth + 1,
    "auth=%s-%s|dataset=%s|encoding=dbn|ts_out=%d\n",
    hex,
    bucket_id,
    dataset,
    ts_out ? 1 : 0);

  send(dbn->sock, auth, strlen(auth), 0);

  free(auth);


  /*
   * Receive third control message.
   */
  char *msg2 = receive_control_message(dbn->sock);
  if (!msg2)
  {
    invoke_error_handler(
      dbn,
      true,
      "Error receiving third control message");
    errno = EBADMSG;
    return -1;
  }


  char *success = get_control_message_field(msg2, "success");
  free(msg2);
  if (!success)
  {
    invoke_error_handler(
      dbn,
      true,
      "Third control message is missing success field");
    errno = EBADMSG;
    return -1;
  }

  if (strcmp(success, "1"))
  {
    invoke_error_handler(
      dbn,
      true,
      "Databento authentication failed");
    free(success);
    errno = EACCES;
    return -1;
  }

  free(success);


  /*
   * Connection is up and authenticated, ready to subscribe.
   */
  return 0;
}


int dbn_start(
  dbn_t *dbn,
  const char *schema,
  const char *symbology,
  int num_roots,
  const char * const *roots,
  const char *suffix,
  bool replay)
{
  /*
   * Subscribing to all symbols means subscribing only to the special
   * ALL_SYMBOLS symbol. Suffix is ignored.
   */
  if (num_roots == 0)
  {
    char *subscribe;
    if (replay)
    {
      int n = snprintf(NULL, 0, "schema=%s|stype_in=%s|start=0|symbols=ALL_SYMBOLS\n", schema, symbology);
      subscribe = malloc(n + 1);
      memset(subscribe, 0, n + 1);
      snprintf(subscribe, n + 1, "schema=%s|stype_in=%s|start=0|symbols=ALL_SYMBOLS\n", schema, symbology);
    }
    else
    {
      int n = snprintf(NULL, 0, "schema=%s|stype_in=%s|symbols=ALL_SYMBOLS\n", schema, symbology);
      subscribe = malloc(n + 1);
      memset(subscribe, 0, n + 1);
      snprintf(subscribe, n + 1, "schema=%s|stype_in=%s|symbols=ALL_SYMBOLS\n", schema, symbology);
    }

    send(dbn->sock, subscribe, strlen(subscribe), 0);

    free(subscribe);
  }


  /*
   * Otherwise we have to subscribe in chunks of up to 1000 symbols at a time
   * (Databento limitation).
   */
  else for (int i = 0; i < num_roots; i += 1000)
  {
    int num_roots_i = num_roots - i;
    if (num_roots_i > 1000) num_roots_i = 1000;

    bool is_last = i + 1000 >= num_roots;


    /*
     * Build the subscription string.
     */
    char *subscribe;
    if (replay)
    {
      int n = snprintf(NULL, 0, "schema=%s|stype_in=%s|start=0|is_last=%s|symbols=", schema, symbology, is_last ? "1" : "0");
      subscribe = malloc(n + 1);
      memset(subscribe, 0, n + 1);
      snprintf(subscribe, n + 1, "schema=%s|stype_in=%s|start=0|is_last=%s|symbols=", schema, symbology, is_last ? "1" : "0");
    }
    else
    {
      int n = snprintf(NULL, 0, "schema=%s|stype_in=%s|is_last=%s|symbols=", schema, symbology, is_last ? "1" : "0");
      subscribe = malloc(n + 1);
      memset(subscribe, 0, n + 1);
      snprintf(subscribe, n + 1, "schema=%s|stype_in=%s|is_last=%s|symbols=", schema, symbology, is_last ? "1" : "0");
    }

    for (int j = 0; j < num_roots_i; j++)
    {
      if (j == num_roots_i - 1) // Last symbol in this chunk
      {
        int m = snprintf(NULL, 0, "%s%s%s\n", subscribe, roots[i + j], suffix);
        char *t = malloc(m + 1);
        memset(t, 0, m + 1);
        snprintf(t, m + 1, "%s%s%s\n", subscribe, roots[i + j], suffix);
        free(subscribe);
        subscribe = t;
      }
      else // Not the last symbol in this chunk
      {
        int m = snprintf(NULL, 0, "%s%s%s,", subscribe, roots[i + j], suffix);
        char *t = malloc(m + 1);
        memset(t, 0, m + 1);
        snprintf(t, m + 1, "%s%s%s,", subscribe, roots[i + j], suffix);
        free(subscribe);
        subscribe = t;
      }
    }


    /*
     * Subscribe to this chunk.
     */
    send(dbn->sock, subscribe, strlen(subscribe), 0);
    free(subscribe);
  }


  /*
   * Start the streaming session. All subsequent data received will
   * be DBN-encoded.
   */
  char start[] = "start_session=0\n";
  send(dbn->sock, start, strlen(start), 0);


  /*
   * Receive the DBN stream header.
   */
  uint8_t preheader[8];
  int preheader_head = 0;

  while (preheader_head < 8)
  {
    ssize_t m = recv(dbn->sock, preheader + preheader_head, 8 - preheader_head, 0);
    if (m > 0)
    {
      preheader_head += m;
    }
    else if (m == 0)
    {
      invoke_error_handler(
        dbn,
        true,
        "Connection closed unexpectedly");
      errno = ECONNRESET;
      return -1;
    }
    else
    {
      int e = errno;
      invoke_error_handler(
        dbn,
        true,
        "Error reading from socket (errno %d: %s)",
        e,
        strerror(e));
      errno = e;
      return -1;
    }
  }

  if (strncmp((char *)preheader, "DBN", 3))
  {
    invoke_error_handler(
      dbn,
      true,
      "Stream header has invalid signature");
    errno = EBADMSG;
    return -1;
  }

  if (preheader[3] != 1)
  {
    invoke_error_handler(
      dbn,
      true,
      "Stream header version %d unsupported",
      preheader[3]);
    errno = EBADMSG;
    return -1;
  }

  int header_length = (int)*(uint32_t *)(preheader + 4);


  /*
   * Receive the rest of the DBN stream header.
   */
  uint8_t *header = malloc(header_length);
  int header_head = 0;
  while (header_head < header_length)
  {
    ssize_t m = recv(
      dbn->sock,
      header + header_head,
      header_length - header_head,
      0);
    if (m > 0)
    {
      header_head += m;
    }
    else if (m == 0)
    {
      invoke_error_handler(
        dbn,
        true,
        "Connection closed unexpectedly");
      free(header);
      errno = ECONNRESET;
      return -1;
    }
    else
    {
      int e = errno;
      invoke_error_handler(
        dbn,
        true,
        "Error reading from socket (errno %d: %s)",
        e,
        strerror(e));
      free(header);
      errno = e;
      return -1;
    }
  }

  free(header);


  /*
   * DBN-encoded messages will be received now. Submit a read request for each
   * of our two buffers. Tag each read with the buffer for reference later.
   */
  struct io_uring_sqe *sqe = io_uring_get_sqe(&dbn->ring);
  io_uring_prep_recv(sqe, dbn->sock, dbn->buffer0, dbn->capacity, 0);
  io_uring_sqe_set_data(sqe, dbn->buffer0);

  sqe = io_uring_get_sqe(&dbn->ring);
  io_uring_prep_recv(sqe, dbn->sock, dbn->buffer1, dbn->capacity, 0);
  io_uring_sqe_set_data(sqe, dbn->buffer1);

  io_uring_submit(&dbn->ring);

  return 0;
}


int dbn_get(dbn_t *dbn)
{
  /*
   * Wait for some data to arrive in one of our two io_uring buffers.
   */
  struct io_uring_cqe *cqe;
  int m = io_uring_wait_cqe(&dbn->ring, &cqe);
  if (m < 0)
  {
    if (m == -EINTR) return 0;
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Error waiting on io_uring (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return -1;
  }

  void *buffer = io_uring_cqe_get_data(cqe);
  ssize_t n = cqe->res;
  io_uring_cqe_seen(&dbn->ring, cqe);

  if (n == 0)
  {
    invoke_error_handler(
      dbn,
      true,
      "Connection closed unexpectedly");
    errno = ECONNRESET;
    return -1;
  }
  else if (n < 0)
  {
    int e = errno;
    invoke_error_handler(
      dbn,
      true,
      "Error reading from socket (errno %d: %s)",
      e,
      strerror(e));
    errno = e;
    return -1;
  }


  /*
   * If we have leftover data from a previous read, copy it into the buffer
   * into which the latest data arrives. This is slow, but luckily it is very
   * rare, because:
   *  (1) Each TCP packet contains an integral number of Databento messages.
   *  (2) Our buffers and the kernel receiver buffer far exceed Databento message size.
   *
   * In fact, this can ONLY happen due to timing between userland calls, kernel DMA,
   * and NIC DMA.
   */
  if (dbn->leftover_count)
  {
    if (dbn->leftover_count + n > dbn->capacity)
    {
      invoke_error_handler(
        dbn,
        true,
        "Leftover data would cause buffer overflow");
      errno = ENOMEM;
      return -1;
    }

    memmove(buffer + dbn->leftover_count, buffer, n);
    memcpy(buffer, dbn->leftover, dbn->leftover_count);
    n += dbn->leftover_count;
    dbn->leftover_count = 0;
  }


  /*
   * Decode as many messages as we can, and dispatch them.
   */
  uint8_t *ptr = buffer;
  int num_messages;
  for (num_messages = 0; ; num_messages++)
  {
    if (n < 16) break; // Not enough data for a header

    int rlength = 4 * ptr[0];
    if (rlength < 16)
    {
      invoke_error_handler(
        dbn,
        true,
        "Bad message length %d",
        rlength);
      errno = EBADMSG;
      return -1;
    }
    if (n < rlength) break; // Not enough data for this message

    if (dbn->on_msg) dbn->on_msg(
      dbn,
      (dbn_hdr_t *)ptr);

    ptr += rlength;
    n -= rlength;
  }


  /*
   * Keep any leftover data. See comments earlier in this function for
   * more info.
   */
  if (n)
  {
    memcpy(dbn->leftover, ptr, n);
    dbn->leftover_count = n;
  }


  /*
   * Re-enqueue this buffer for more data.
   */
  struct io_uring_sqe *sqe = io_uring_get_sqe(&dbn->ring);
  io_uring_prep_recv(sqe, dbn->sock, buffer, dbn->capacity, 0);
  io_uring_sqe_set_data(sqe, buffer);

  io_uring_submit(&dbn->ring);

  return num_messages;
}


void dbn_close(dbn_t *dbn)
{
  io_uring_queue_exit(&dbn->ring);

  close(dbn->sock);

  if (dbn->buffer0) free(dbn->buffer0);
  if (dbn->buffer1) free(dbn->buffer1);
  if (dbn->leftover) free(dbn->leftover);

  memset(dbn, 0, sizeof(dbn_t));
}
