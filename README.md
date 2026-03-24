# HTTP Request Prediction for Faster Web Response using OMNeT++

An OMNeT++ simulation project that models HTTP browsing behavior and evaluates **pattern-based predictive caching** under different workloads and cache configurations.

## What this project implements

### Core features
- Multi-client HTTP request simulation over a configurable network.
- Mixed browsing behavior:
  - **80% predictable navigation** pattern (`home -> login -> dashboard`)
  - **20% random page selection**
- Server-side predictive caching with:
  - transition learning from observed navigation
  - configurable prediction threshold
  - TTL-based cache expiry
  - LRU eviction when cache is full
- Rich metrics collection for latency, cache efficiency, and prediction impact.

### Implemented algorithms
1. **Transition-based next-page prediction (Markov-style first-order model)**
   - Learns transition counts `(fromPage, toPage)`.
   - Computes transition probability: `P(to|from) = count(from->to) / sum_x count(from->x)`.
   - Uses `predictionThreshold` to decide whether to pre-cache predicted pages.

2. **Predictive pre-caching**
   - After serving requests, the server predicts likely next pages and pre-populates cache entries for pages above threshold.

3. **Cache lifecycle management**
   - **TTL expiry** for staleness control.
   - **Periodic cleanup** of expired entries.
   - **LRU eviction** to enforce `maxCacheSize`.

4. **Cache entry ranking utilities**
   - `CacheEntry` includes comparators for:
     - LRU (`lastAccess`)
     - LFU (`accessCount`)
     - FIFO (`timestamp`)
   - Current server eviction path uses **LRU**.

---

## Codebase index

### Root
- `Makefile` - top-level build entry points (`make`, `makefiles`, `clean`, `cleanall`).
- `simulations/omnetpp.ini` - primary experiment configurations and parameter sweeps.
- `simulations/run` - helper script to run compiled simulation binary.

### Source (`src/`)
- `HttpNetwork.ned` - network topology (server + configurable number of clients, channels).
- `HttpServer.ned` - server module parameters, signals, and statistics declarations.
- `HttpServer.cc` - request handling, pattern learning, predictive caching, cache/TTL/LRU management.
- `HttpClient.ned` - client module wiring.
- `HttpClient.cc` - client traffic behavior (80/20 pattern vs random), request scheduling, response timing.
- `HttpMessage.h/.cc` - HTTP request/response message models.
- `PatternTable.h/.cc` - transition table, probability computation, prediction APIs, cache of predictions.
- `CacheEntry.h/.cc` - cache item metadata and expiry/access helpers.
- `omnetpp.ini` - source-level OMNeT++ config.

---

## Architecture summary

- **Clients (`HttpClient`)** generate requests and maintain page context (`fromPage`) so the server can learn transitions.
- **Server (`HttpServer`)**:
  - serves six pages (`home`, `login`, `dashboard`, `profile`, `settings`, `logout`)
  - checks response cache first (fast path)
  - processes misses with longer delay
  - updates transition patterns and triggers predictive pre-caching
- **Pattern engine (`PatternTable`)** computes confidence-ranked next-page candidates.
- **Cache (`CacheEntry` + server map)** tracks content, TTL, timestamps, and access stats.

---

## Build and run

> Prerequisite: OMNeT++ toolchain must be installed and available in your shell (`opp_makemake`, `opp_configfilepath`, runtime binaries).

### Build
```bash
make makefiles
make
```

### Clean
```bash
make clean
make cleanall
```

### Run examples
```bash
# Using the provided helper script (runs ../src/HttpPredictiveCache)
./simulations/run -u Cmdenv -c Baseline simulations/omnetpp.ini

# Baseline (prediction effectively disabled)
omnetpp -u Cmdenv -c Baseline simulations/omnetpp.ini

# Predictive caching (default tuned configuration)
omnetpp -u Cmdenv -c Predictive simulations/omnetpp.ini

# Parameter sweep (threshold x TTL combinations)
omnetpp -u Cmdenv -c Sweep simulations/omnetpp.ini

# GUI mode
omnetpp -u Qtenv -c Predictive simulations/omnetpp.ini
```

---

## Available simulation configurations

Defined in `simulations/omnetpp.ini`:
- `Baseline`
- `Predictive`
- `Sweep`
- `HighLoad`
- `LongTerm`
- `QuickTest`
- `Aggressive`
- `Conservative`
- `Standard` (legacy baseline-like standard setup)

Key tunables:
- `*.server.predictionThreshold`
- `*.server.cacheTTL`
- `*.server.maxCacheSize`
- `*.numClients`
- `sim-time-limit`

---

## Metrics captured

Server-side statistics include:
- requests received / responses generated
- processing delay and response time
- cache hits, misses, hit-rate
- pre-generated (predictively cached) pages
- cache expiry and eviction counts
- estimated time savings from cache hits

Client-side statistics include:
- requests sent / responses received
- response times
- pattern-followed vs random request behavior signals

---

## Expected behavior at a glance

- **Baseline**: near-zero predictive benefit (threshold > 1), higher average latency.
- **Predictive**: improved cache hit rate and lower response times for recurrent navigation patterns.
- **Sweep/Aggressive/Conservative**: lets you study threshold/TTL trade-offs between cache hit ratio, freshness, and unnecessary pre-caching.
