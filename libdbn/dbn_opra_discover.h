/**
 * @file dbn_opra_discover.h
 * @brief Databento client wrapper that discovers options and optionable roots
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


/**
 * @brief Option discovery state
 */
typedef enum
{
  DBN_OPRA_DISCOVER_STATE_NOT_STARTED = 0,  ///< @brief Client initialized but not connected yet
  DBN_OPRA_DISCOVER_STATE_CONNECTED,        ///< @brief Client connected and subscribing
  DBN_OPRA_DISCOVER_STATE_SUBSCRIBED,       ///< @brief Client subscribed and receiving security definitions
  DBN_OPRA_DISCOVER_STATE_XREF,             ///< @brief Client is cross-referencing security definitions to symbols
  DBN_OPRA_DISCOVER_STATE_DONE,             ///< @brief Client finished and ready to disconnect / close
  DBN_OPRA_DISCOVER_STATE_ERROR             ///< @brief Client errored out
} dbn_opra_discover_state_t;


/**
 * @brief Instrument ID to option contract OSI (OCC) symbol.
 */
typedef struct
{
  uint32_t   instrument_id;  ///< @brief Databento instrument ID, only reliable within the same trading day
  osi_t      symbol;         ///< @brief OSI (OCC) option symbol
  dbn_sdef_t *sdef;          ///< @brief Pointer to security definition
} dbn_opra_discover_option_t;


/**
 * @brief Discovered information about an optionable root.
 */
typedef struct
{
  char *root;                          ///< @brief Root symbol without .OPT suffix (ex. "MSFT", "SPY")
  size_t cap_options;                  ///< @brief Capacity of the options list
  size_t num_options;                  ///< @brief Number of options discovered for this root
  dbn_opra_discover_option_t *options; ///< @brief Discovered options for this root
} dbn_opra_discover_root_t;


/**
 * @brief Number of buckets in an instrument ID to sdef map.
 *
 * There are approx. 7000 optionable symbols as of this writing. Some have
 * as few as 100 options available, and others have many thousands. Anything
 * from 25000 to 100000 is a decent choice for number of buckets.
 */
#define DBN_OPRA_DISCOVER_NUM_SDEF_BUCKETS 50000


/**
 * @brief Bucket in an instrument ID to sdef map.
 */
typedef struct
{
  size_t capacity;       ///< @brief Allocated slots in this bucket
  size_t count;          ///< @brief Used slots in this bucket
  dbn_sdef_t *sdefs;     ///< @brief Slots
} dbn_opra_discover_sdef_bucket_t;


/**
 * @brief Databento client wrapper that discovers options and optionable roots.
 */
typedef struct
{
  dbn_t dbn;                        ///< @brief Underlying client
  dbn_opra_discover_state_t state;  ///< @brief State
  size_t num_roots;                 ///< @brief Number of discovered optionable roots
  dbn_opra_discover_root_t *roots;  ///< @brief Optionable roots and their contracts, do not read unless state is DONE
  char *error;                      ///< @brief Error message, for state ERROR. NULL in other states.
  pthread_t thread;                 ///< @brief Worker thread
  bool stop;                        ///< @brief Stop flag for worker thread
  size_t num_options;               ///< @brief Total number of options discovered
  size_t num_sdefs;                 ///< @brief Total number of security definitions received
  dbn_opra_discover_sdef_bucket_t sdefs[DBN_OPRA_DISCOVER_NUM_SDEF_BUCKETS]; ///< @brief Maps instrument ID to security definition
} dbn_opra_discover_t;


/**
 * @brief Initialize an OPRA discovery client wrapper but don't start yet.
 *
 * @param discover Pointer to an uninitialized client wrapper object.
 */
extern void dbn_opra_discover_init(
  dbn_opra_discover_t *discover);


/**
 * @brief Connect to Databento and start discovering options.
 *
 * @param discover Pointer to an initialized client wrapper object.
 * @param api_key Pointer to null-terminated Databento API key.
 *
 * @return 0 on success, or -1 on failure with errno set and error handler
 * invoked (if not NULL).
 */
extern int dbn_opra_discover_start(
  dbn_opra_discover_t *discover,
  const char *api_key);


/**
 * @brief Stop / disconnect from Databento and destroy a client wrapper
 * object.
 *
 * @param discover Pointer to an initialized client wrapper object.
 *
 * It is safe to call this even if dbn_discover_start() fails or an error
 * occurs.
 */
extern void dbn_opra_discover_destroy(dbn_opra_discover_t *discover);

