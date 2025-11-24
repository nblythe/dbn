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
[ 16%] Building C object dbn/CMakeFiles/dbn.dir/dbn.c.o
[ 33%] Linking C static library libdbn.a
[ 33%] Built target dbn
[ 50%] Building C object dbn_stats/CMakeFiles/dbn_stats.dir/dbn_stats.c.o
[ 66%] Linking C executable dbn_stats
[ 66%] Built target dbn_stats
[ 83%] Building C object dbn_roots/CMakeFiles/dbn_roots.dir/dbn_roots.c.o
[100%] Linking C executable dbn_roots
[100%] Built target dbn_roots
$
```

## Build outputs
This project includes three binaries:
- `libdbn.a`: Databento real-time market data client, static library. See `dbn/dbn/README.md` for details.
- `dbn_stats`: Susbcribes to command-line specified data and collects message counts and timing statistics. See `dbn/dbn_stats/README.md` for details.
- `dbn_roots`: Collects and prints Databento-supported optionable equity root symbols (ex. `MSFT.OPT`, `SPY.OPT`, etc.). See `dbn/dbn_roots/README.md` for details.
