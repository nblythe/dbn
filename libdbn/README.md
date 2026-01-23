# Databento live market data client
**(C) 2025 Nathan Blythe**
\
**Released under the Apache-2.0 license, see LICENSE**

## Overview
This library provides two clients for receiving Databento live market data. `dbn_t` uses blocking calls and callbacks to receive and dispatch Databento live market data messages. `dbn_multi_t` wraps multiple parallel `dbn_t` clients, for multi-threaded / multi-session data streaming.


## Using `dbn_t`

A connection to Databento is represented by a `dbn_t` client object, which contains a TCP socket file descriptor, buffers, and two handler callback functions.

The error handler is called when something goes wrong in the client itself.

```
void on_error(
  dbn_t *dbn,
  bool fatal,
  char *msg)
{
  if (fatal)
  {
    printf("Error: %s\n", msg);
    exit(EXIT_FAILURE);
  }
  else printf("Warning: %s\n", msg);
}
```

The message handler is called when a message is received from Databento. The message types that might be received depend on the dataset and schema.

```
void on_msg(
  dbn_t *dbn,
  dbn_msg_t *msg)
{
  if (msg->type == DBN_RTYPE_SMAP) // Symbol mapping message
  {
    dbn_smap_t *smap = (void *)msg;
    printf(
      "%u: SMAP %s\n",
      smap->hdr.instrument_id,
      smap->stype_out_symbol);
  }
  else if (msg->type == DBN_RTYPE_CMBP1)
  {
    dbn_cmbp1_t *cmbp1 = (void *)msg;
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
}
```

First declare a `dbn_t` client object, call `dbn_init()`. Provide the handler functions you defined (or `NULL`). You can also provide an arbitrary context pointer that will be stored in the `dbn_t` object, for reference later during callbacks.

```
dbn_t dbn;
dbn_init(&dbn, on_error, on_msg, NULL);
```

Connect to Databento by calling `dbn_connect()`, specifying your API key and the dataset you wish to connect to.

```
if (dbn_connect(
  &dbn,
  "my api key here",
  "OPRA.PILLAR",
  false))
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
  false))
{
  perror("dbn_start");
  exit(EXIT_FAILURE);
}
```

Once subscribed, Databento will begin sending data, which will buffer within the `dbn_t` object's buffers. (This client uses `liburing` and the kernel's `io_uring` mechanism to receive data into a pair of user space buffers asynchronously.) To process this data, you must call `dbn_get()`. Generally you will want to dedicate a thread to repeatedly calling `dbn_get()` to process messages as fast as possible. Each call will block until at least one message is received and processed. Under high load, many messages may be processed by a single call to `dbn_get()`. Messages result in the assigned handler function being called. `dbn_get()` itself returns the number of messages that were processed by that call.

```
while(1)
{
  int n = dbn_get(&dbn);
  printf("Received %d messages\n", n);
}
```

Finally, to close the connection and free memory within the client object, call `dbn_close()`. The `dbn_t` client object is unitialized after this call.

A single `dbn_t` is sufficient for most datasets and schemas.


## Using dbn_multi_t

`dbn_multi_t` encapsulates one or more `dbn_t` objects, with worker threads that handle calling `dbn_start()` and `dbn_get()`. The interface is very similar to that of `dbn_t`.


The error handler is called when something goes wrong in any `dbn_t` client.

```
void on_error(
  dbn_multi_t *dbn_multi,
  bool fatal,
  char *msg)
{
  if (fatal)
  {
    printf("Error: %s\n", msg);
    exit(EXIT_FAILURE);
  }
  else printf("Warning: %s\n", msg);
}
```

The message handler is called when a message is received from Databento on any `dbn_t` client.

```
void on_msg(
  dbn_multi_t *dbn_multi,
  dbn_msg_t *msg)
{
  if (msg->type == DBN_RTYPE_SMAP) // Symbol mapping message
  {
    dbn_smap_t *smap = (void *)msg;
    printf(
      "%u: SMAP %s\n",
      smap->hdr.instrument_id,
      smap->stype_out_symbol);
  }
  else if (msg->type == DBN_RTYPE_CMBP1)
  {
    dbn_cmbp1_t *cmbp1 = (void *)msg;
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
}
```

First declare a `dbn_multi_t` client object, call `dbn_multi_init()`. Provide the handler functions you defined (or `NULL`). You can also provide an arbitrary context pointer that will be stored in the `dbn_multi_t` object, for reference later during callbacks.

```
dbn_multi_t dbn_multi;
dbn_multi_init(&dbn_multi, on_error, on_msg, NULL);
```

Connect an individual client / thread / session to Databento and suscribe to data by calling `dbn_multi_connect_and_start`, specifying your API key, the dataset you wish to connect to, and the schema, symbology, and symbols you want to stream. Like in `dbn_start()`, if the symbols need a common suffix, you can specify that, too.

This function will return as soon as the underlying `dbn_connect()` is complete, but does not wait for `dbn_start()` to complete. The `dbn_start()` call happens in the worker thread.

```
size_t n = sizeof(symbols) / sizeof(char *);

if (dbn_multi_connect_and_start(
  &dbn_multi,
  "my api key here",
  "OPRA.PILLAR",
  false,
  "cmbp-1",
  "parent",
  n / 2,
  symbols,
  ".OPT",
  false))
{
  perror("dbn_multi_connect_and_start (1 of 2)");
  exit(EXIT_FAILURE);
}


if (dbn_multi_connect_and_start(
  &dbn_multi,
  "my api key here",
  "OPRA.PILLAR",
  false,
  "cmbp-1",
  "parent",
  n / 2,
  &symbols[n / 2],
  ".OPT",
  false))
{
  perror("dbn_multi_connect_and_start (2 of 2)");
  exit(EXIT_FAILURE);
}
```

Call `dbn_multi_is_fully_subscribed()` to determine if all created clients / sessions / threads have completed their calls to `dbn_start()`. Note that some clients / sessions / threads may begin receiving messages (meaning that the `on_message` callback will be invoked) while others are still subscribing.

```
while (!dbn_multi_is_fully_subscribed(&dbn_multi))
  usleep(100000); // 100 ms
```

Finally, to close all connections, stop all threads, and free all memory within all client objects, call `dbn_multi_close_all()`. The `dbn_multi_t` client object is unitialized after this call.

Each client's worker thread calls `dbn_get()` as quickly as possible once that client is subscribed. The assigned `on_msg` (and, if necessary, `on_error`) callbacks are called from the worker thread, without synchronization.


## Performance
For maximum performance, adhere to the following guidelines:
1. Set the kernel maximum TCP receive buffer size to at least 64 MiB. (ex. `sysctl -w net.core.rmem-max=67108864`)
2. Do as little work in message handlers as possible. For `dbn_multi_t`, consider using the `on_msg` callback to enqueue tasks into lock-free queues from which separate worker threads can draw.

The most data-intensive stream Databento offers is the OPRA.PILLAR dataset with CMBP-1 schema. This client has been clocked consuming over 4 million OPRA quotes per second in intra-day replay mode with a 3 Gbps circuit and 10 CPU-pinned, channel-sharded connections / threads. With 4 ms ping latency to opra-pillar.lsg.databento.com, this client has demonstrated < 10 ms message latency under a load of 2.5 million quotes per second during a live market session, measured by the difference between ntp-synced host time and Databento message `ts_out` time.
