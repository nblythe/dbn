# Subscribe to Databento market data and collect statistics    
**(C) 2025 Nathan Blythe**
\
**Released under the Apache-2.0 license, see LICENSE**

## Overview
This program subscribes to the dataset, schema, and symbols specified by the user on the command line and receives messages from Databento, collecting count and timing information. Once the user kills the program with SIGINT / CTRL-C, it prints out a summary of the information.

## Usage
```
dbn_stats -k <key> -d <dataset> -c <schema> -b <symbology> [-s <symbol>] [-f <path>] [-r] [-h]
```
- `-k <key>`: Databento API key (required)
- `-d <dataset>`: Dataset name (required)
- `-c <schema>`: Schema (required)
- `-b <symbology>`: Symbology (required)
- `-s <symbol>`: Symbol (optional, may provide multiple)
- `-f <path>`: Path to file of symbols, one per line (optional, may provide multiple)
- `-r`: Use intra-day replay instead of real-time data
- `-h`: Show usage information and exit

Once subscribed, the program will collect statistics until killed with SIGINT / CTRL-C. RAM usage will increase steadily, especially when subscribing to high-throughput datasets for many symbols. 30 - 60 seconds is a sufficient runtime to get useful measurements.

## Examples

Real-time data:
```
$ ./dbn_stats -k my_key -d OPRA.PILLAR -c cmbp-1 -b parent -s MSFT.OPT -s AAPL.OPT
Connecting to Databento... OK
Subscribing to 2 symbols from dataset OPRA.PILLAR, schema cmbp-1... OK
Running... ^C
Timing:
  Connect time:           83.805 ms
  Subscribe time:         2.132 s
  Symbol mapping time:    27.767 ms
  Data time:              53.807 s
  Total run time:         56.051 s
Message counts:
  emsg:  0
  smsg:  1
  smap:  6096
  sdef:  0
  cmbp1: 1513814
  bbo:   0
Message rates:
  smap:  219.545 thousand messages per second
  sdef:  0.000 messages per second
  cmpb1: 28.134 thousand messages per second
  bbo:   0.000 messages per second
Latencies:
  ts_event -> ts_recv:  201.014 us
  ts_event -> ts_out:   2.023 ms
  ts_recv  -> ts_out:   1.822 ms
  ts_out   -> ts_local: 3.447 ms
  ts_event -> ts_local: 5.470 ms
  ts_recv  -> ts_local: 5.269 ms
```

Intra-day replay:
```
$ ./dbn_stats -k my_key -d OPRA.PILLAR -c cbbbo-1s -b parent -s MSFT.OPT -s AAPL.OPT -r
Connecting to Databento... OK
Subscribing to 2 symbols from dataset OPRA.PILLAR, schema cbbo-1s... OK
Running... ^C
Timing:
  Connect time:           294.925 ms
  Subscribe time:         2.124 s
  Symbol mapping time:    888.986 ms
  Data time:              54.019 s
  Total run time:         57.328 s
Message counts:
  emsg:  0
  smsg:  74992
  smap:  12192
  sdef:  0
  cmbp1: 0
  bbo:   9577752
Message rates:
  smap:  13.714 thousand messages per second
  sdef:  0.000 messages per second
  cmpb1: 0.000 messages per second
  bbo:   177.303 thousand messages per second
Latencies:
  ts_event -> ts_recv:  n/a (intra-day replay)
  ts_event -> ts_out:   n/a (intra-day replay)
  ts_recv  -> ts_out:   n/a (intra-day replay)
  ts_out   -> ts_local: 3.901 ms
  ts_event -> ts_local: n/a (intra-day replay)
  ts_recv  -> ts_local: n/a (intra-day replay)
```

## Interpreting results

Message rates are calculated over the respective window. For example, symbol mapping messages are only received during the symbol mapping phase of a connection, so the symbol mapping rate measures how many smap messages were received per second during that phase, not over the whole session.

Interpretation of latencies depends on the dataset:
- `ts_event -> ts_recv`: For CMBP-1 this measures the duration from when a quote or trade occurred (as timestamped by the venue) to when the quote/trade was received by Databento (as timestamped by the Databento feed handler). For interval-based BBO this measures the duration from when the quote/trade occurred to the end of the interval.
- `ts_event -> ts_out`: Measures the duration from when a quote or trade occurred (as timestamped by the venue) to when the quote/trade was dispatched to the user (as timestamped by the Databento gateway). For CMBP-1 this is a measure of internal latency in Databento's feed handler and gateway, combined. For interval-based BBO this is not very useful, since the latest quote for any given symbol could be quite a long time ago.
- `ts_recv -> ts_out`: For CMBP-1 this measures the duration from when a quote/trade was received by Databento (as timestamped by the Databento feed handler) to when the quote/trade was dispatched to the user (as timestamped by the Databento gateway), and is a measure of internal latency in Databento's feed handler and gateway, combined. For interval-based BBO this measures the duration from the end of the interval to when the quote was dispatched to the user, and is a measure of internal latency in Databento's gateway.
- `ts_out -> ts_local`: Measures the duration from when a quote or trade was dispatched to the user (as timestamped by Databento's gateway) and when it was received by the client (as timestamped locally). This is a measure of path latency between Databento and the user.
- `ts_event -> ts_local`: Measures the duration from when a quote or trade occurred (as timestamped by the venue) to when it was received by the user (as timestamped locally). For CMBP-1 this is a measure of total latency of the data path from venue to user software. For interval-based BBO this is not very useful.
- `ts_recv -> ts_local`: Measures the duration from when a quote or trade was received by Databento (as timestamped by the Databento feed handler) to when it was received by the client (as timestamped locally). This is the best measure of combined Databento feed handler, Databento gateway, and path latency.

`ts_local` is effected by host clock accuracy and jitter; for a typical host with NTP, accuracy is probably 1-2 milliseconds. For example, a reported `ts_out -> ts_local` of 5.841 ms could probably be safely interpreted as "sub 8 ms path latency".

In intra-day replay, `ts_event` and `ts_recv` are not useful for measuring latency, since the quotes and trades from which they are captured are historical, so the corresponding average latencies are not calculated.
