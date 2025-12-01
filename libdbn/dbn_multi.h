/**
 * @file dbn_multi.h
 * @brief Multi-threaded, multi-session Databento live market data client
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for example usage.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <pthread.h>

#include "dbn.h"


/*
 * Forward reference to client object.
 */
struct dbn_multi;
typedef struct dbn_multi dbn_multi_t;


/**
 * @brief Signature for an error handler.
 *
 * @param dbn_multi Pointer to client object.
 * @param fatal Indicates if the error is fatal, and further comms are unlikely to succeed.
 * @param msg Pointer to null-terminated error message.
 */
typedef void (*dbn_multi_on_error_t)(
  dbn_multi_t *dbn_multi,
  bool fatal,
  char *msg);


/**
 * @brief Signature for a Databento message handler.
 *
 * @param dbn_multi Pointer to client object.
 * @param msg Pointer to message. Handler must not free, and must not rely on it after the handler returns.
 */
typedef void (*dbn_multi_on_msg_t)(
  dbn_multi_t *dbn_multi,
  dbn_hdr_t *msg);


/**
 * @brief Multi-threaded, multi-session Databento live data client
 */
struct dbn_multi
{
  int num_sessions;                 ///< @brief Number of parallel clients / threads
  dbn_t **clients;                  ///< @brief Underlying clients, one per session
  pthread_t *threads;               ///< @brief Threads, one per session
  bool stop;                        ///< @brief Stop flag for threads
  _Atomic uint64_t num_subscribed;  ///< @brief Number of sessions subscribed to their symbols
  dbn_multi_on_error_t on_error;    ///< @brief If not NULL, called on runtime client error
  dbn_multi_on_msg_t on_msg;        ///< @brief If not NULL, called on receipt of a Databento message
  void *ctx;                        ///< @brief Optional, arbitrary owner-provided pointer associated with this client
};


/**
 * @brief Initialize a multi-threaded, mult-session Databento live data
 * client, but don't connect any sessions yet.
 *
 * @param dbn_multi Pointer to an uninitialized client object.
 * @param on_error Error handler. May be NULL.
 * @param on_msg Message handler. May be NULL.
 * @param ctx Optional, arbitrary pointer to associate with this client. May be NULL.
 */
extern void dbn_multi_init(
  dbn_multi_t *dbn_multi,
  dbn_multi_on_error_t on_error,
  dbn_multi_on_msg_t on_msg,
  void *ctx);


/**
 * @brief Establish a new parallel session / thread with Databento,
 * authenticate, and subscribe to one or more symbols.
 *
 * @param dbn_multi Pointer to an initialized client object.
 * @param api_key Pointer to null-terminated Databento API key.
 * @param dataset Pointer to null-terminated Databento dataset name.
 * @param ts_out Indicates if Databento should perform ts_out timestamping.
 * @param schema Pointer to null-terminated schema name.
 * @param symbology Pointer to null-terminated symbology name.
 * @param num_symbols Number of symbols in symbols.
 * @param symbols Pointer to array of pointers to null-terminated symbols.
 * @param suffix Pointer to null-terminated suffix string to be applied to symbols.
 * @param replay If true, client will replay the current day's worth of data instead of subscribing to live data.
 *
 * @return 0 on success, or -1 on failure with errno set and error handler
 * invoked (if not NULL).
 */
extern int dbn_multi_connect_and_start(
  dbn_multi_t *dbn_multi,
  const char *api_key,
  const char *dataset,
  bool ts_out,
  const char *schema,
  const char *symbology,
  int num_symbols,
  const char * const *symbols,
  const char *suffix,
  bool replay);


/**
 * @brief Determine if all sessions in a multi-session client are subscribed.
 *
 * @param dbn_multi Pointer to an initialized client object.
 *
 * @return true if all sessions are subscribed, false if some are still subscribing.
 */
extern bool dbn_multi_is_fully_subscribed(
  dbn_multi_t *dbn_multi);


/**
 * @brief Disconnect all sessions from Databento and free any allocated memory.
 *
 * @param dbn_multi Pointer to an initialized client object.
 *
 * It is safe to call this even if dbn_multi_connect_and_start() fails.
 */
extern void dbn_multi_close_all(dbn_multi_t *dbn_multi);

