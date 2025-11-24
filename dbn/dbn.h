/**
 * @file dbn.h
 * @brief Databento live market data client
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for example usage.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <liburing.h>


/**
 * @brief DBN message header.
 */
typedef struct __attribute__((packed))
{
  uint8_t rlength;
  uint8_t rtype;
  uint16_t publisher_id;
  uint32_t instrument_id;
  uint64_t ts_event;
} dbn_hdr_t;


/**
 * @brief DBN symbol mapping message.
 */
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  char stype_in_symbol[22];
  char stype_out_symbol[22];
  uint8_t dummy[4];
  uint64_t start_ts;
  uint64_t end_ts;
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_smap_t;


/**
 * @brief DBN security definition message.
 */
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  uint64_t ts_recv;
  int64_t min_price_increment;
  int64_t display_factor;
  uint64_t expiration;
  uint64_t activation;
  int64_t high_limit_price;
  int64_t low_limit_price;
  int64_t max_price_variation;
  int64_t trading_reference_price;
  int64_t unit_of_measure_qty;
  int64_t min_price_increment_amount;
  int64_t price_ratio;
  int32_t inst_attrib_value;
  uint32_t underlying_id;
  uint32_t raw_instrument_id;
  int32_t market_depth_implied;
  int32_t market_depth;
  uint32_t market_segment_id;
  uint32_t max_trade_vol;
  int32_t min_lot_size;
  int32_t min_lot_size_block;
  int32_t min_lot_size_round_lot;
  uint32_t min_trade_vol;
  uint8_t _reserved2[4];
  int32_t contract_multiplier;
  int32_t decay_quantity;
  int32_t original_contract_size;
  uint8_t _reserved3[4];
  uint16_t trading_reference_date;
  int16_t appl_id;
  uint16_t maturity_year;
  uint16_t decay_start_date;
  uint16_t channel_id;
  char currency[4];
  char settl_currency[4];
  char secsubtype[6];
  char raw_symbol[22];
  char group[21];
  char exchange[5];
  char asset[7];
  char cfi[7];
  char security_type[7];
  char unit_of_measure[31];
  char underlying[21];
  char strike_price_currency[4];
  uint32_t instrument_class;
  uint8_t _reserved4[2];
  int64_t strike_price;
  uint8_t _reserved5[6];
  uint32_t match_algorithm;
  uint8_t md_security_trading_status;
  uint8_t main_fraction;
  uint8_t price_display_format;
  uint8_t settl_price_type;
  uint8_t sub_fraction;
  uint8_t underlying_product;
  uint32_t security_update_action;
  uint8_t maturity_month;
  uint8_t maturity_day;
  uint8_t maturity_week;
  uint32_t user_defined_instrument;
  int8_t contract_multiplier_unit;
  int8_t flow_schedule_type;
  uint8_t tick_rule;
  uint8_t _dummy[3];
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_sdef_t;


/**
 * @brief DBN CMBP-1 message.
 */
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  int64_t price;
  uint32_t size;
  char action;
  char side;
  uint8_t flags;
  uint8_t reserved1;
  uint64_t ts_recv;
  int32_t ts_in_delta;
  int32_t reserved2;
  uint64_t bid_px;
  uint64_t ask_px;
  uint32_t bid_sz;
  uint32_t ask_sz;
  uint16_t bid_pb;
  uint16_t reserved3;
  uint16_t ask_pb;
  uint16_t reserved4;
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_cmbp1_t;


/**
 * @brief DBN BBO message.
 */
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  int64_t price;
  uint32_t size;
  uint8_t reserved1;
  char side;
  uint8_t flags;
  uint8_t reserved2;
  uint64_t ts_recv;
  uint32_t reserved3;
  uint32_t sequence;
  uint64_t bid_px;
  uint64_t ask_px;
  uint32_t bid_sz;
  uint32_t ask_sz;
  uint32_t bid_ct;
  uint32_t ask_ct;
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_bbo_t;


/**
 * @brief DBN error message.
 */
typedef struct
{
  dbn_hdr_t hdr;
  char msg[64];
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_emsg_t;


/**
 * @brief DBN system message.
 */
typedef struct
{
  dbn_hdr_t hdr;
  char msg[64];
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_smsg_t;


/*
 * Forward reference to client object.
 */
struct dbn;
typedef struct dbn dbn_t;


/**
 * @brief Signature for an error handler.
 *
 * @param dbn Pointer to client object.
 * @param fatal Indicates if the error is fatal, and further comms are unlikely to succeed.
 * @param msg Pointer to null-terminated error message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_error_handler_t)(
  dbn_t *dbn,
  bool fatal,
  char *msg,
  void *arg);


/**
 * @brief Signature for a symbol mapping message handler.
 *
 * @param dbn Pointer to client object.
 * @param smap Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_smap_handler_t)(
  dbn_t *dbn,
  dbn_smap_t *smap,
  void *arg);


/**
 * @brief Signature for a security definition message handler.
 *
 * @param dbn Pointer to client object.
 * @param sdef Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_sdef_handler_t)(
  dbn_t *dbn,
  dbn_sdef_t *sdef,
  void *arg);


/**
 * @brief Signature for a CMBP-1 message handler.
 *
 * @param dbn Pointer to client object.
 * @param cmbp1 Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_cmbp1_handler_t)(
  dbn_t *dbn,
  dbn_cmbp1_t *cmbp1,
  void *arg);


/**
 * @brief Signature for a BBO message handler.
 *
 * @param dbn Pointer to client object.
 * @param bbo Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_bbo_handler_t)(
  dbn_t *dbn,
  dbn_bbo_t *bbo,
  void *arg);


/**
 * @brief Signature for an error message handler.
 *
 * @param dbn Pointer to client object.
 * @param emsg Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_emsg_handler_t)(
  dbn_t *dbn,
  dbn_emsg_t *emsg,
  void *arg);


/**
 * @brief Signature for a system message handler.
 *
 * @param dbn Pointer to client object.
 * @param smsg Pointer to message.
 * @param arg Arbitrary caller-provided pointer provided to the function invoking this handler.
 */
typedef void (*dbn_smsg_handler_t)(
  dbn_t *dbn,
  dbn_smsg_t *smsg,
  void *arg);


/**
 * @brief Databento live data client
 */
struct dbn
{
  int sock;                       ///< @brief Socket file descriptor
  int capacity;                   ///< @brief Kernel receive buffer size, and size of local buffers, in bytes
  struct io_uring ring;           ///< @brief io_uring used to communicate with the socket
  uint8_t *buffer0;               ///< @brief First receive buffer, to be filled by the kernel while the client is handling data in the second buffer
  uint8_t *buffer1;               ///< @brief Second receive buffer, to be filled by the kernel while the client is handling data in the first buffer
  uint8_t *leftover;              ///< @brief Leftover data buffer, used to hold incomplete message data that spans multiple io_uring reads
  int leftover_count;             ///< @brief Number of bytes in the leftover data buffer
  dbn_error_handler_t on_error;   ///< @brief If not NULL, called on runtime client error
  dbn_smap_handler_t on_smap;     ///< @brief If not NULL, called on receipt of a symbol mapping message
  dbn_sdef_handler_t on_sdef;     ///< @brief If not null, called on receipt of a security definition message
  dbn_cmbp1_handler_t on_cmbp1;   ///< @brief If not NULL, called on receipt of a cmbp-1 message
  dbn_bbo_handler_t on_bbo;       ///< @brief If not NULL, called on receipt of a bbo-1s or bbo-1m message
  dbn_emsg_handler_t on_emsg;     ///< @brief If not NULL, called on receipt of an error message
  dbn_smsg_handler_t on_smsg;     ///< @brief If not NULL, called on receipt of a system message
};


/**
 * @brief Initialize a Databento live data client, but don't connect yet.
 *
 * @param dbn Pointer to an uninitialized client object.
 */
extern void dbn_init(
  dbn_t *dbn);


/**
 * @brief Establish a connection to Databento and authenticate.
 *
 * @param dbn Pointer to an initialized client object.
 * @param api_key Pointer to null-terminated Databento API key.
 * @param dataset Pointer to null-terminated Databento dataset name.
 * @param ts_out Indicates if Databento should perform ts_out timestamping.
 * @param arg Arbitrary pointer passed along to any handlers invoked by this function.
 *
 * @return 0 on success, or -1 on failure with errno set and error handler
 * invoked (if not NULL).
 */
extern int dbn_connect(
  dbn_t *dbn,
  const char *api_key,
  const char *dataset,
  bool ts_out,
  void *arg);


/**
 * @brief Start streaming data for one or more symbols.
 *
 * @param dbn Pointer to an initialized client object.
 * @param schema Pointer to null-terminated schema name.
 * @param symbology Pointer to null-terminated symbology name.
 * @param num_symbols Number of symbols in symbols.
 * @param symbols Pointer to array of pointers to null-terminated symbols.
 * @param suffix Pointer to null-terminated suffix string to be applied to symbols.
 * @param replay If true, client will replay the current day's worth of data instead of subscribing to live data.
 * @param arg Arbitrary pointer passed along to any handlers invoked by this function.
 *
 * @return 0 on success, or -1 on failure with errno set and error handler invoked (if not NULL).
 */
extern int dbn_start(
  dbn_t *dbn,
  const char *schema,
  const char *symbology,
  int num_symbols,
  const char * const *symbols,
  const char *suffix,
  bool replay,
  void *arg);


/**
 * @brief Receive data from Databento. Blocks until at least one message is received.
 *
 * @param dbn Pointer to an initialized and started client object.
 * @param arg Arbitrary pointer passed along to any handlers invoked by this function.
 *
 * @return Number of messages received by this call.
 */
extern int dbn_get(dbn_t *dbn, void *arg);


/**
 * @brief Disconnect from Databento and free any allocated memory.
 *
 * @param dbn Pointer to an initialized client object.
 *
 * It is safe to call this even if dbn_connect() fails.
 */
extern void dbn_close(dbn_t *dbn);

