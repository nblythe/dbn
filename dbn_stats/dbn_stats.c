/**
 * @file dbn_stats.c
 * @brief Subscribe to command-line specified data and collect statistics
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <dbn.h>


/**
 * @brief Show usage and exit.
 *
 * @param exit_code Exit code with which to call exit().
 *
 * Does not return.
 */
static void usage(int exit_code)
{
  printf("Usage: dbn_stats -k <key> -d <dataset> -c <schema> -b <symbology> [-s <symbol>] [-f <path>] [-r] [-h]\n");
  printf("\n");
  printf("Options:\n");
  printf("   -k <key>       Databento API key\n");
  printf("   -d <dataset>   Dataset name\n");
  printf("   -c <schema>    Schema name\n");
  printf("   -b <symbology> Symbology\n");
  printf("   -s <symbol>    Symbol (may provide multiple)\n");
  printf("   -f <path>      Path to file of symbols, one per line (may provide multiple)\n");
  printf("   -r             Intra-day replay\n");
  printf("   -h             Show this usage information and exit\n");
  printf("\n");
  printf("Example: dbn_stats -k <key> -d OPRA.PILLAR -c cbbo-1s -b parent -s MSFT.OPT -s AAPL.OPT\n");
  exit(exit_code);
}


/**
 * @brief Databento live data client.
 */
static dbn_t dbn;


/**
 * @brief Flag used to stop the program on SIGINT.
 */
static volatile bool siginted = false;


/*
 * Statistics collected while running.
 */
static volatile uint64_t num_emsg = 0;
static volatile uint64_t num_smsg = 0;
static volatile uint64_t ts_smap_first = 0;
static volatile uint64_t ts_smap_last = 0;
static volatile uint64_t num_smap = 0;
static volatile uint64_t num_sdef = 0;
static volatile uint64_t num_cmbp1 = 0;
static volatile uint64_t num_bbo = 0;
static uint64_t *tss_event = NULL;
static uint64_t *tss_recv = NULL;
static uint64_t *tss_out = NULL;
static uint64_t *tss_local = NULL;
static volatile uint64_t tss_capacity = 0;
static volatile uint64_t tss_count = 0;


/**
 * @brief Record a ts_event / ts_recv / ts_out / ts_local triplet.
 *
 * @param ts_event ts_event from quote message (CMBP-1 or BBO)
 * @param ts_recv ts_recv from quote message (CMBP-1 or BBO)
 * @param ts_out ts_out from quote message (CMBP-1 or BBO)
 * @param ts_local Local timestamp
 */
static inline void record_timestamps(
  uint64_t ts_event,
  uint64_t ts_recv,
  uint64_t ts_out,
  uint64_t ts_local)
{
  if (tss_count == tss_capacity)
  {
    tss_capacity = tss_capacity ? 2 * tss_capacity : 1048576;

    tss_event = realloc(tss_event, tss_capacity * sizeof(uint64_t));
    if (!tss_event)
    {
      perror("realloc");
      abort();
    }

    tss_recv = realloc(tss_recv, tss_capacity * sizeof(uint64_t));
    if (!tss_recv)
    {
      perror("realloc");
      abort();
    }

    tss_out = realloc(tss_out, tss_capacity * sizeof(uint64_t));
    if (!tss_out)
    {
      perror("realloc");
      abort();
    }

    tss_local = realloc(tss_local, tss_capacity * sizeof(uint64_t));
    if (!tss_local)
    {
      perror("realloc");
      abort();
    }
  }

  tss_event[tss_count] = ts_event;
  tss_recv[tss_count] = ts_recv;
  tss_out[tss_count] = ts_out;
  tss_local[tss_count] = ts_local;
  tss_count++;
}


/**
 * @brief SIGINT handler.
 */
static void on_sigint(int signum)
{
  static int num_sigints = 0;

  siginted = true;
  num_sigints++;

  if (num_sigints == 2)
  {
    dbn_close(&dbn);
  }
  else if (num_sigints > 2)
    abort();
}


/**
 * @brief Get current time in Unix nanoseconds.
 */
static inline uint64_t nanotime()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
  {
    perror("clock_gettime");
    abort();
  }
  return ts.tv_sec * 1000000000ul + ts.tv_nsec;
}


/**
 * @brief Get nanoseconds in a friendly string, with units.
 *
 * @param ns Nanoseconds
 *
 * @return Pointer to friendly string. Do not free.
 */
static const char *pptime(uint64_t ns)
{
  static char text[256];
  const int n = sizeof(text) - 1;

  if (ns < 1000) snprintf(text, n, "%lu ns", ns);
  else if (ns < 1000000)
  {
    double us = ns / 1000.0;
    snprintf(text, n, "%.3f us", us);
  }
  else if (ns < 1000000000)
  {
    double ms = ns / 1000000.0;
    snprintf(text, n, "%.3f ms", ms);
  }
  else if (ns < 60000000000)
  {
    double s = ns / 1000000000.0;
    snprintf(text, n, "%.3f s", s);
  }
  else
  {
    double m = ns / 60000000000.0;
    snprintf(text, n, "%.3f m", m);
  }

  return text;
}


/**
 * @brief Get messages-per-second in a friendly string, with units.
 *
 * @param count Total number of messages
 * @param ns Total nanoseconds elapsed
 *
 * @return Pointer to friendly string. Do not free.
 */
static const char *pprate(uint64_t count, uint64_t ns)
{
  static char text[256];
  const int n = sizeof(text) - 1;

  double ps = count * 1000000000.0 / ns;
  double kps = count * 1000000.0 / ns;
  double mps = count * 1000.0 / ns;

  if (mps > 1) snprintf(text, n, "%.3f million messages per second", mps);
  else if (kps > 1) snprintf(text, n, "%.3f thousand messages per second", kps);
  else snprintf(text, n, "%.3f messages per second", ps);

  return text;
}


/**
 * @brief Handle client errors and warnings by printing to stdout.
 */
static void on_error(dbn_t *dbn, bool fatal, char *msg, void *arg)
{
  if (fatal)
  {
    fprintf(stderr, "Client error: %s\n", msg);
    exit(EXIT_FAILURE);
  }
  else
    fprintf(stderr, "Client warning: %s\n", msg);
}


/**
 * @brief Count server errors and print to stdout.
 */
static void on_emsg(dbn_t *dbn, dbn_emsg_t *emsg, void *arg)
{
  fprintf(stderr, "Server error: %s\n", emsg->msg);
  num_emsg++;
}


/**
 * @brief Count system messages.
 */
static void on_smsg(dbn_t *dbn, dbn_smsg_t *smsg, void *arg)
{
  num_smsg++;
}


/**
 * @brief Count symbol mapping messages and record first and last
 * time received.
 */
static void on_smap(dbn_t *dbn, dbn_smap_t *smap, void *arg)
{
  num_smap++;
  if (!ts_smap_first) ts_smap_first = nanotime();
  else ts_smap_last = nanotime();
}


/**
 * @brief Count symbol definition messages.
 */
static void on_sdef(dbn_t *dbn, dbn_sdef_t *sdef, void *arg)
{
  num_sdef++;
}


/**
 * @brief Count CMBP-1 messages and record timestamps for calculating
 * latencies.
 */
static void on_cmbp1(dbn_t *dbn, dbn_cmbp1_t *cmbp1, void *arg)
{
  num_cmbp1++;
  record_timestamps(cmbp1->hdr.ts_event, cmbp1->ts_recv, cmbp1->ts_out, nanotime());
}


/**
 * @brief Count BBO messages and record timestamps for calculating latencies.
 */
static void on_bbo(dbn_t *dbn, dbn_bbo_t *bbo, void *arg)
{
  num_bbo++;
  record_timestamps(bbo->hdr.ts_event, bbo->ts_recv, bbo->ts_out, nanotime());
}


int main(int argc, char **argv)
{
  /*
   * Parse args.
   */
  char *api_key = NULL;
  char *dataset = NULL;
  char *schema = NULL;
  char *symbology = NULL;
  char **symbols = NULL;
  int num_symbols = 0;
  bool replay = false;
  char c;
  while ((c = getopt(argc, argv, "hk:d:c:b:s:f:r")) != -1)
  {
    switch(c)
    {
      case 'h':
        usage(EXIT_SUCCESS);
      case 'k':
        api_key = optarg;
        break;
      case 'd':
        dataset = optarg;
        break;
      case 'c':
        schema = optarg;
        break;
      case 'b':
        symbology = optarg;
        break;
      case 's':
        num_symbols++;
        symbols = realloc(symbols, num_symbols * sizeof(char *));
        if (!symbols)
        {
          perror("realloc");
          abort();
        }
        symbols[num_symbols - 1] = optarg;
        break;
      case 'f':
        int fd = open(optarg, O_RDONLY);
        if (fd < 0)
        {
          fprintf(stderr, "Failed to open %s : %s\n", optarg, strerror(errno));
          exit(EXIT_FAILURE);
        }

        while (true)
        {
          bool eof = false;
          char *symbol = calloc(1, 64);
          for (int i = 0; i < 63; i++)
          {
            char c;
            int n = read(fd, &c, 1);
            if (n < 0)
            {
              perror("read");
              abort();
            }
            else if (!n)
            {
              eof = true;
              break;
            }
            else if (c == '\n') break;
            else symbol[i] = c;
          }
          if (symbol[0])
          {
            num_symbols++;
            symbols = realloc(symbols, num_symbols * sizeof(char *));
            if (!symbols)
            {
              perror("realloc");
              abort();
            }
            symbols[num_symbols - 1] = symbol;
          }

          if (eof) break;
        }

        close(fd);
        break;
      case 'r':
        replay = true;
        break;
      case '?':
      default:
        usage(EXIT_FAILURE);
    }
  }

  if (!api_key || !dataset || !schema || !symbology || !num_symbols)
    usage(EXIT_FAILURE);


  /*
   * Register sigint handler.
   */
  signal(SIGINT, on_sigint);


  /*
   * Create a client and register handlers for all supported messages.
   */
  dbn_t dbn;
  dbn_init(&dbn);
  dbn.on_error = on_error;
  dbn.on_smsg = on_smsg;
  dbn.on_emsg = on_emsg;
  dbn.on_smap = on_smap;
  dbn.on_sdef = on_sdef;
  dbn.on_cmbp1 = on_cmbp1;
  dbn.on_bbo = on_bbo;


  /*
   * Connect.
   */
  printf("Connecting to Databento... ");
  fflush(stdout);

  uint64_t ts_connect_start = nanotime();

  if (dbn_connect(
    &dbn,
    api_key,
    dataset,
    true,
    NULL)) exit(EXIT_FAILURE);

  uint64_t ts_connect_end = nanotime();
  printf("OK\n");


  /*
   * Subscribe.
   */
  printf("Subscribing to %d symbol%s from dataset %s, schema %s... ",
    num_symbols,
    num_symbols == 1 ? "" : "s",
    dataset,
    schema);
  fflush(stdout);

  uint64_t ts_subscribe_start = nanotime();

  if (dbn_start(
    &dbn,
    schema,
    symbology,
    num_symbols,
    (const char * const*)symbols,
    "",
    replay,
    NULL)) exit(EXIT_FAILURE);

  uint64_t ts_subscribe_end = nanotime();
  printf("OK\n");


  /*
   * Run until sigint.
   */
  printf("Running... ");
  fflush(stdout);

  while (!siginted)
  {
    dbn_get(&dbn, NULL);
  }

  uint64_t ts_run_end = nanotime();
  printf("\n");


  /*
   * Disconnect and free.
   */
  dbn_close(&dbn);


  /*
   * Summarize statistics.
   */
  printf("Timing:\n");
  printf("  Connect time:           %s\n", pptime(ts_connect_end - ts_connect_start));
  printf("  Subscribe time:         %s\n", pptime(ts_subscribe_end - ts_subscribe_start));
  printf("  Symbol mapping time:    %s\n", pptime(ts_smap_last - ts_smap_first));
  printf("  Data time:              %s\n", pptime(ts_run_end - ts_smap_last));
  printf("  Total run time:         %s\n", pptime(ts_run_end - ts_connect_start));

  printf("Message counts:\n");
  printf("  emsg:  %ld\n", num_emsg);
  printf("  smsg:  %ld\n", num_smsg);
  printf("  smap:  %ld\n", num_smap);
  printf("  sdef:  %ld\n", num_sdef);
  printf("  cmbp1: %ld\n", num_cmbp1);
  printf("  bbo:   %ld\n", num_bbo);

  printf("Message rates:\n");
  printf("  smap:  %s\n", pprate(num_smap, ts_smap_last - ts_smap_first));
  printf("  sdef:  %s\n", pprate(num_sdef, ts_run_end - ts_smap_last));
  printf("  cmpb1: %s\n", pprate(num_cmbp1, ts_run_end - ts_smap_last));
  printf("  bbo:   %s\n", pprate(num_bbo, ts_run_end - ts_smap_last));

  double ts_event_recv = 0;
  double ts_event_out = 0;
  double ts_recv_out = 0;
  double ts_out_local = 0;
  double ts_event_local = 0;
  double ts_recv_local = 0;
  for (size_t i = 0; i < tss_count; i++)
  {
    ts_event_recv += tss_recv[i] - tss_event[i];
    ts_event_out += tss_out[i] - tss_event[i];
    ts_recv_out += tss_out[i] - tss_recv[i];
    ts_out_local += tss_local[i] - tss_out[i];
    ts_event_local += tss_local[i] - tss_event[i];
    ts_recv_local += tss_local[i] - tss_recv[i];
  }
  ts_event_recv /= tss_count;
  ts_event_out /= tss_count;
  ts_recv_out /= tss_count;
  ts_out_local /= tss_count;
  ts_event_local /= tss_count;
  ts_recv_local /= tss_count;


  printf("Latencies:\n");

  if (replay)
  {
    printf("  ts_event -> ts_recv:  n/a (intra-day replay)\n");
    printf("  ts_event -> ts_out:   n/a (intra-day replay)\n");
    printf("  ts_recv  -> ts_out:   n/a (intra-day replay)\n");
  }
  else
  {
    printf("  ts_event -> ts_recv:  %s\n", pptime((uint64_t)ts_event_recv));
    printf("  ts_event -> ts_out:   %s\n", pptime((uint64_t)ts_event_out));
    printf("  ts_recv  -> ts_out:   %s\n", pptime((uint64_t)ts_recv_out));
  }

  printf("  ts_out   -> ts_local: %s\n", pptime((uint64_t)ts_out_local));

  if (replay)
  {
    printf("  ts_event -> ts_local: n/a (intra-day replay)\n");
    printf("  ts_recv  -> ts_local: n/a (intra-day replay)\n");
  }
  else
  {
    printf("  ts_event -> ts_local: %s\n", pptime((uint64_t)ts_event_local));
    printf("  ts_recv  -> ts_local: %s\n", pptime((uint64_t)ts_recv_local));
  }

  return EXIT_SUCCESS;
}
