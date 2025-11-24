/**
 * @file osi.h
 * @brief OCC Symbology (OSI)
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * This file provides osi_parse() to parse OCC (OSI) option contract symbols.
 *
 * Symbols must have correctly padded roots; i.e. "TSLA  250815C00100000", not
 * "TSLA250815C00100000".
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Decoded OSI symbol.
 *
 * Packing is optional. It's included here in case you want to send this
 * struct over the wire.
 */
typedef struct __attribute__((packed))
{
  char root[7];      ///< @brief Root symbol (bytes 0 through 6)
  uint8_t exp_year;  ///< @brief Expiration year, since 2000 (byte 7)
  uint8_t exp_month; ///< @brief Expiration month (1 - 12) (byte 8)
  uint8_t exp_day;   ///< @brief Expiration day (1 - 31) (byte 9)
  bool is_call;      ///< @brief Call or put (byte 10)
  char pad[5];       ///< @brief Padding to 8-byte align the strike (bytes 11 through 15)
  uint64_t strike;   ///< @brief Strike price in nanodollars (bytes 16 through 23)
} osi_t;


/*
 * Parse an OCC (OSI) option contract symbol.
 *
 * Takes:
 *   symbol: Pointer to null-terminated OCC (OSI) option contract symbol.
 *           Must be exactly 21 ASCII characters plus null terminator.
 *   osi: Pointer where decoded symbol will be stored on success.
 *
 * Returns:
 *   true on successful parsing, or false otherwise.
 */
static inline bool osi_parse(
  const char *symbol,
  osi_t *osi)
{
  if (strlen(symbol) != 21) return false;

  osi->root[6] = 0;
  memcpy(osi->root, symbol, 6);
  for (int i = 0; i <= 6; i++)
  {
    if (osi->root[i] == ' ') osi->root[i] = 0;
  }

  char t[3];
  t[2] = 0;
  t[0] = symbol[6];
  t[1] = symbol[7];
  osi->exp_year = strtol(t, NULL, 10);

  t[0] = symbol[8];
  t[1] = symbol[9];
  osi->exp_month = strtol(t, NULL, 10);

  t[0] = symbol[10];
  t[1] = symbol[11];
  osi->exp_day = strtol(t, NULL, 10);

  osi->is_call = symbol[12] == 'C' ? true : false;

  char s[9];
  s[8] = 0;
  memcpy(s, symbol + 13, 8);
  osi->strike = 1000000L * strtol(s, NULL, 10);

  return true;
}
