# Databento live market data C client and sample applications  
**(C) 2025 Nathan Blythe**
\
**Released under the Apache-2.0 license, see LICENSE**

## Build requirements
- CMake 3.10 or newer
- A modern C compiler (`gcc` or `clang`, usually)
- `libsodium` (used in Databento authentication)
- `liburing` (for double-buffered socket I/O)
- `pkg-config` (for use by CMake to locate `libsodium` and `liburing`)

On Debian and Ubuntu `apt`-based systems, build requirements can be installed with:
```
# apt install build-essential cmake pkg-config libsodium-dev liburing-dev
```

## Building
```
$ cd dbn
$ cmake . -DCMAKE_BUILD_TYPE=Release
-- The C compiler identification is GNU 12.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Found PkgConfig: /usr/bin/pkg-config (found version "1.8.1") 
-- Checking for module 'libsodium'
--   Found libsodium, version 1.0.18
-- Checking for module 'liburing'
--   Found liburing, version 2.3
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/dbn
$ make
[ 16%] Building C object libdbn/CMakeFiles/libdbn.dir/dbn.c.o
[ 33%] Linking C static library libdbn.a
[ 33%] Built target dbn
...
$
```

## Build outputs
This project produces five outputs:
- `libdbn.a`: Databento real-time market data client, static library.
- `libdbnopra.a`: Client wrappers for working with Databento's OPRA.PILLAR dataset.
- `dbn_stats`: Susbcribes to command-line specified data and collects message counts and timing statistics.
- `dbn_multi_stats`: Multi-threaded, multi-session version of `dbn_stats`.
- `dbn_roots`: Collects and prints Databento-supported optionable equity root symbols (ex. `MSFT.OPT`, `SPY.OPT`, etc.).
- `dbn_opra_stress`: Subscribes to the entire OPRA.PILLAR CMBP-1 equity option dataset, a stress-test of host and network performance.
