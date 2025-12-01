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
 * @brief DBN message types.
 */
typedef enum
{
  DBN_RTYPE_MBP0      = 0x00,
  DBN_RTYPE_MBP1      = 0x01,
  DBN_RTYPE_MBP10     = 0x0A,
  DBN_RTYPE_STATUS    = 0x12,
  DBN_RTYPE_SDEF      = 0x13,
  DBN_RTYPE_IMBALANCE = 0x14,
  DBN_RTYPE_EMSG      = 0x15,
  DBN_RTYPE_SMAP      = 0x16,
  DBN_RTYPE_SMSG      = 0x17,
  DBN_RTYPE_STAT      = 0x18,
  DBN_RTYPE_OHLCV1S   = 0x20,
  DBN_RTYPE_OHLCV1M   = 0x21,
  DBN_RTYPE_OHLCV1H   = 0x22,
  DBN_RTYPE_OHLCV1D   = 0x23,
  DBN_RTYPE_MBO       = 0xA0,
  DBN_RTYPE_CMBP1     = 0xB1,
  DBN_RTYPE_CBBO1S    = 0xC0,
  DBN_RTYPE_CBBO1M    = 0xC1,
  DBN_RTYPE_TCBBO     = 0xC2,
  DBN_RTYPE_BBO1S     = 0xC3,
  DBN_RTYPE_BBO1M     = 0xC4
} dbn_rtype_t;


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
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  char msg[64];
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_emsg_t;


/**
 * @brief DBN system message.
 */
typedef struct __attribute__((packed))
{
  dbn_hdr_t hdr;
  char msg[64];
  uint64_t ts_out; // Only valid if ts_out enabled during authentication
} dbn_smsg_t;


/*
 * Macros supporting DBN_MAX_MESSAGE_SIZE computation.
 */
#define MAX2(a, b)    ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define MAX6(a, b, c, d, e, f) MAX2(MAX3(a, b, c), MAX3(d, e, f))


/**
 * @brief Maximum size of a supported DBN message, compile-time constant.
 */
#define DBN_MAX_MESSAGE_SIZE MAX6( \
  sizeof(dbn_smap_t), \
  sizeof(dbn_sdef_t), \
  sizeof(dbn_cmbp1_t), \
  sizeof(dbn_bbo_t), \
  sizeof(dbn_emsg_t), \
  sizeof(dbn_smsg_t))


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
 */
typedef void (*dbn_on_error_t)(
  dbn_t *dbn,
  bool fatal,
  char *msg);


/**
 * @brief Signature for a Databento message handler.
 *
 * @param dbn Pointer to client object.
 * @param msg Pointer to message. Handler must not free, and must not rely on it after the handler returns.
 */
typedef void (*dbn_on_msg_t)(
  dbn_t *dbn,
  dbn_hdr_t *msg);


/**
 * @brief Databento live data client
 */
struct dbn
{
  int sock;                   ///< @brief Socket file descriptor
  int capacity;               ///< @brief Kernel receive buffer size, and size of local buffers, in bytes
  struct io_uring ring;       ///< @brief io_uring used to communicate with the socket
  uint8_t *buffer0;           ///< @brief First receive buffer, to be filled by the kernel while the client is handling data in the second buffer
  uint8_t *buffer1;           ///< @brief Second receive buffer, to be filled by the kernel while the client is handling data in the first buffer
  uint8_t *leftover;          ///< @brief Leftover data buffer, used to hold incomplete message data that spans multiple io_uring reads
  int leftover_count;         ///< @brief Number of bytes in the leftover data buffer
  dbn_on_error_t on_error;    ///< @brief If not NULL, called on runtime client error
  dbn_on_msg_t on_msg;        ///< @brief If not NULL, called on receipt of a Databento message
  void *ctx;                  ///< @brief Optional, arbitrary owner-provided pointer
};


/**
 * @brief Initialize a Databento live data client, but don't connect yet.
 *
 * @param dbn Pointer to an uninitialized client object.
 * @param on_error Pointer to client error handler. May be NULL.
 * @param on_msg Pointer to client message handler. May be NULL.
 * @param ctx Optional, arbitrary pointer to associate with the client.
 */
extern void dbn_init(
  dbn_t *dbn,
  dbn_on_error_t on_error,
  dbn_on_msg_t on_msg,
  void *ctx);


/**
 * @brief Establish a connection to Databento and authenticate.
 *
 * @param dbn Pointer to an initialized client object.
 * @param api_key Pointer to null-terminated Databento API key.
 * @param dataset Pointer to null-terminated Databento dataset name.
 * @param ts_out Indicates if Databento should perform ts_out timestamping.
 *
 * @return 0 on success, or -1 on failure with errno set and error handler
 * invoked (if not NULL).
 */
extern int dbn_connect(
  dbn_t *dbn,
  const char *api_key,
  const char *dataset,
  bool ts_out);


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
  bool replay);


/**
 * @brief Receive data from Databento. Blocks until at least one message is received.
 *
 * @param dbn Pointer to an initialized and started client object.
 *
 * @return Number of messages received by this call.
 */
extern int dbn_get(dbn_t *dbn);


/**
 * @brief Disconnect from Databento and free any allocated memory.
 *
 * @param dbn Pointer to an initialized client object.
 *
 * It is safe to call this even if dbn_connect() fails.
 */
extern void dbn_close(dbn_t *dbn);

