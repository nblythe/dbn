# Discover optionable equity root symbols on Databento     
**(C) 2025 Nathan Blythe**
\
**Released under the Apache-2.0 license, see LICENSE**


## Overview
This program discovers all optionable equity root symbols supported on Databento, such as MSFT, AAPL, and SPY. It subscribes to the OPRA.PILLAR dataset, for the ALL_SYMBOLS meta-symbol, definition schema, and intra-day replay. This will send
one symbol mapping message and one security definition message for every option contract that traded within the current market day, followed by a system message indicating that replay is complete. The root symbol is extracted from the symbol
mapping messages and stored, sorted. Once all symbols have been received, the sorted list is dumped either to standard output or to a file.

Root symbols are dumped with the .OPT suffix (ex. MSFT.OPT, AAPL.OPT, SPY.OPT) so that they can be used with Databento's "parent" symbology.

Note that the program does not actually use the security definition messages for anything. It has to subscribe to *something*, and since there's a limited number of security definition messages (no more than the number of contracts) and we ge
t a system message when they're all sent, it's a good choice.     

## Usage
```
dbn_roots -k <key> [-h] [-o <path>]
```


- `-k <key>`: Databento API key (required)
- `-c`: Dump as a C header instead of a simple list
- `-o <path>`: Dump roots to file instead of standard output
- `-h`: Show usage information and exit

## Example
```
$ ./dbn_roots -k my_key
Connecting to Databento... OK
Subscribing to ALL_SYMBOLS from OPRA.PILLAR dataset, definition schema, intra-day replay... OK
Collecting roots... 6149 (3529392 options)
Writing roots to stdout:
A.OPT
AA.OPT
AAAU.OPT
AAL.OPT
...
```
