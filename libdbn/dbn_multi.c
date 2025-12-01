/**
 * @file dbn_multi.c
 * @brief Multi-threaded, multi-session Databento live market data client
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for example usage.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#include "dbn.h"
#include "dbn_multi.h"


/**
 * @brief Worker thread start argument.
 */
typedef struct
{
  dbn_multi_t *dbn_multi;         ///< @brief Back-pointer to the dbn_multi_t client with which this thread is associated.
  dbn_t *dbn;                     ///< @brief dbn_t client with which this thread is associated.
  const char *schema;             ///< @brief Schema to subscribe to.
  const char *symbology;          ///< @brief Symbology name.
  int num_symbols;                ///< @brief Number of symbols this thread / client / session will subscribe to.
  const char * const * symbols;   ///< @brief Symbols this thread / client / session will subscribe to.
  const char *suffix;             ///< @brief Common suffix for symbols (ex. ".OPT")
  bool replay;                    ///< @brief If true, this thread / client / session will use intra-day replay.
} thread_arg_t;


/**
 * @brief Worker thread entry point.
 */
static void *thread(void *arg)
{
  thread_arg_t *thread_arg = arg;
  dbn_multi_t *dbn_multi = thread_arg->dbn_multi;
  dbn_t *dbn = thread_arg->dbn;

  if (dbn_start(
    dbn,
    thread_arg->schema,
    thread_arg->symbology,
    thread_arg->num_symbols,
    thread_arg->symbols,
    thread_arg->suffix,
    thread_arg->replay))
    return NULL;

  atomic_fetch_add(&dbn_multi->num_subscribed, 1);

  free(arg);

  while (!dbn_multi->stop)
  {
    dbn_get(dbn);
  }

  return NULL;
}


/**
 * @brief On dbn_t client error, invoke the dbn_multi_t-scope client error
 * handler.
 */
static void on_error(
  dbn_t *dbn,
  bool fatal,
  char *msg)
{
  dbn_multi_t *dbn_multi = dbn->ctx;
  if (dbn_multi->on_error) dbn_multi->on_error(
    dbn_multi,
    fatal,
    msg);
}


/**
 * @brief On Databento message, invoke the dbn_multi_t-scope mesasge
 * handler.
 */
static void on_msg(
  dbn_t *dbn,
  dbn_hdr_t *msg)
{
  dbn_multi_t *dbn_multi = dbn->ctx;
  if (dbn_multi->on_msg) dbn_multi->on_msg(
    dbn_multi,
    msg);
}


void dbn_multi_init(
  dbn_multi_t *dbn_multi,
  dbn_multi_on_error_t on_error,
  dbn_multi_on_msg_t on_msg,
  void *ctx)
{
  memset(dbn_multi, 0, sizeof(dbn_multi_t));
  dbn_multi->on_error = on_error;
  dbn_multi->on_msg = on_msg;
  dbn_multi->ctx = ctx;
}


int dbn_multi_connect_and_start(
  dbn_multi_t *dbn_multi,
  const char *api_key,
  const char *dataset,
  bool ts_out,
  const char *schema,
  const char *symbology,
  int num_symbols,
  const char * const *symbols,
  const char *suffix,
  bool replay)
{
  dbn_multi->num_sessions++;
  dbn_multi->clients = realloc(dbn_multi->clients, dbn_multi->num_sessions * sizeof(dbn_t *));

  int i = dbn_multi->num_sessions - 1;
  dbn_multi->clients[i] = malloc(sizeof(dbn_t));
  dbn_init(dbn_multi->clients[i], on_error, on_msg, dbn_multi);

  thread_arg_t *arg = calloc(1, sizeof(thread_arg_t));
  arg->dbn_multi = dbn_multi;
  arg->dbn = dbn_multi->clients[i];
  arg->schema = schema;
  arg->symbology = symbology;
  arg->suffix = suffix;
  arg->num_symbols = num_symbols;
  arg->symbols = symbols;
  arg->replay = replay;

  int r = dbn_connect(
    dbn_multi->clients[dbn_multi->num_sessions - 1],
    api_key,
    dataset,
    ts_out);
  if (r) return r;

  dbn_multi->threads = realloc(dbn_multi->threads, dbn_multi->num_sessions * sizeof(pthread_t));
  pthread_create(&dbn_multi->threads[dbn_multi->num_sessions - 1], NULL, thread, arg);

  return 0;
}


bool dbn_multi_is_fully_subscribed(
  dbn_multi_t *dbn_multi)
{
  return atomic_load(&dbn_multi->num_subscribed) == dbn_multi->num_sessions;
}


void dbn_multi_close_all(dbn_multi_t *dbn_multi)
{
  /*
   * Stop all threads.
   */
  dbn_multi->stop = true;

  for (int i = 0; i < dbn_multi->num_sessions; i++)
    pthread_join(dbn_multi->threads[i], NULL);


  /*
   * Close / disconnect all clients.
   */
  for (int i = 0; i < dbn_multi->num_sessions; i++)
    dbn_close(dbn_multi->clients[i]);


  /*
   * Clean up.
   */
  if (dbn_multi->clients) free(dbn_multi->clients);
  if (dbn_multi->threads) free(dbn_multi->threads);

  memset(dbn_multi, 0, sizeof(dbn_multi_t));
}
