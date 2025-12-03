/**
 * @file osi.c
 * @brief OCC Symbology (OSI)
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See osi.h
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "osi.h"

bool osi_parse(
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
