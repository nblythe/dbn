# Databento live market data client
**(C) 2025 Nathan Blythe**
\
**Released under the Apache-2.0 license, see LICENSE**

## Overview
This client uses blocking calls and callbacks to receive and dispatch Databento live market data messages. A connection to Databento is represented by a `dbn_t` client object, which contains a TCP socket file descriptor, buffers, and a set of handlers for different kinds of messages that might be received.


## Usage
Handlers are just function pointers that are called when a corresponding event occurs. Events you aren't interested in can be left `NULL`.

Start by defining handlers for the different kinds of messages you want to process.

The error handler is a special function that is called when something goes wrong in the client itself.

```
void on_error(
  dbn_t *dbn,
  bool fatal,
  char *msg,
  void *arg)
{
  if (fatal)
  {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
  }
  else fprintf(stderr, "Warning: %s\n", msg);
}
```

All Databento data feeds start with symbol mapping messages that describe how instrument IDs map to symbols.

```
void on_smap(
  dbn_t *dbn,
  dbn_smap_t *smap,
  void *arg)
{
  printf("Instrument ID %u represents %s\n", smap->hdr.instrument_id, smap->stype_out_symbol);
}
```

The CMBP-1 schema provides consolidated top-of-book quotes and trades.

```
void on_cmbp1(
  dbn_t *dbn,
  dbn_cmbp1_t *cmbp1,
  void *arg)
{
  if (cmbp1->action == 'T') printf(
      "%u: TRADE %llu (%u)\n"
      cmbp1->hdr.instrument_id,
      cmbp1->price,
      cmpb1->size);
  else printf(
      "%u: QUOTE %llu (%u) - %llu (%u)\n"
      cmbp1->hdr.instrument_id,
      cmbp1->bid_px,
      cmpb1->bid_sz,
      cmbp1->ask_px,
      cmpb1->ask_sz);
}
```

First declare a `dbn_t` client object, call `dbn_init()`, and assign any handlers you defined.

```
dbn_t dbn;
dbn_init(&dbn);
dbn.on_error = on_error;
dbn.on_smap = on_smap;
dbn.on_cmbp1 = on_cmbp1;
```

Connect to Databento by calling `dbn_connect()`, specifying your API key and the dataset you wish to connect to.

```
dbn_t dbn;
if (dbn_connect(
  &dbn,
  "my api key here",
  "OPRA.PILLAR",
  false,
  NULL))
{
  perror("dbn_connect");
  exit(EXIT_FAILURE);
}
```

Subscribe to data by calling `dbn_start()` and specifying the schema, symbology, and symbols you want to stream. If the symbols need a common suffix, you can specify that, too. If no symbols are specified, the client will subscribe to all supported symbols.

```
const char *symbols[] = { "SPY", "QQQ" };
if(dbn_start(
  &dbn,
  "cmbp-1",
  "parent",
  sizeof(symbols) / sizeof(char *),
  symbols,
  ".OPT",
  false,
  NULL))
{
  perror("dbn_start");
  exit(EXIT_FAILURE);
}
```

Once subscribed, Databento will begin sending data, which will buffer within the `dbn_t` object's buffers. (This client uses `liburing` and the kernel's `io_uring` mechanism to receive data into a pair of user space buffers asynchronously.) To process this data, you must call `dbn_get()`. Generally you will want to dedicate a thread to repeatedly calling `dbn_get()` to process messages as fast as possible. Each call will block until at least one message is received and processed. Under high load, many messages may be processed by a single call to `dbn_get()`. Messages result in the assigned handler functions being called. `dbn_get()` itself returns the number of messages that were processed by that call.

```
while(1)
{
  int n = dbn_get(&dbn, NULL);
  printf("Received %d messages\n", n);
}
```

`dbn_connect()`, `dbn_start()`, and `dbn_get()` all take an arbitrary pointer argument named `arg`. This argument is not used by the client, but is passed along to any handler functions that are invoked. `arg` can be set to NULL if not needed.

Finally, to close the connection and free memory within the client object, call `dbn_close()`. The `dbn_t` client object is unitialized after this call.

## Performance
For maximum performance, adhere to the following guidelines:
1. Set the kernel maximum TCP receive buffer size to at least 64 MiB. (ex. `sysctl -w net.core.rmem-max=67108864`)
2. Use multiple connections, with each connection subscribed to instruments corresponding to a set of Databento channel IDs. (Instrument ID to channel ID mappings can be determined with the "definition" schema.)
3. Use a dedicated CPU-pinned thread for each connection, which calls `dbn_get()` in a loop.
4. Do as little work in message handlers as possible. For computationally intensive tasks, use message handlers to enqueue tasks into lock-free queues from which worker threads can draw.

The most data-intensive stream Databento offers is the OPRA.PILLAR dataset with CMBP-1 schema. This client has been clocked consuming over 4 million OPRA quotes per second in intra-day replay mode with a 3 Gbps circuit and 10 CPU-pinned, channel-sharded connections / threads. With 4 ms ping latency to opra-pillar.lsg.databento.com, this client has demonstrated < 10 ms message latency under a load of 2.5 million quotes per second during a live market session, measured by the difference between ntp-synced host time and Databento message `ts_out` time.
