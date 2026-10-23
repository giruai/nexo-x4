# Decode scheduling experiment — rejected

On 2026-10-23, a local 0.1.11 diagnostic candidate tested whether deferring three idle CPU-copy workers would reduce slow 1080p Decode calls by at least 10%. **That target was not reached.** The candidate was discarded, and the tested 0.1.10 package and source were restored. No 0.1.11 release was published.

## Method

The same diagnostic binary compared deferred workers (B) with the previous eager-worker behavior (A), in B/A/B order. Both modes used the existing direct GPU-to-WB path, a single native decoder owner and depth one. CPU-copy fallback ownership and the eager 720p path were preserved.

The scene was the Fortnite Reload character lobby with the same background. After a 30-second settling period, each sample selected twelve received five-second windows. Decode wall-time histograms used 500-microsecond bins. Paired thread-CPU readings measured only the decoder owner, not driver threads, GPU work or total process CPU.

## Observations

| Sample | Copy workers | Decode calls | Mean Decode wall time | p95 wall-time interval | Mean owner CPU time |
| --- | ---: | ---: | ---: | ---: | ---: |
| B, first | 0 | 1,812 | 10.018 ms | 11.0–11.5 ms | 0.307 ms |
| A, reference | 3 | 1,812 | 10.360 ms | 11.5–12.0 ms | 0.316 ms |
| B, repeat | 0 | 1,813 | 10.050 ms | 11.0–11.5 ms | 0.310 ms |

The intervals describe histogram quantization, not exact percentiles. Even the most favorable interval endpoints do not establish a 10% p95 reduction. Each sample still contained two calls above 33.333 ms. The small mean difference is not evidence of improved gameplay FPS.

The native thread-CPU clock reported 1-microsecond resolution; all selected CPU samples were valid. Probe overhead averaged about 8 microseconds per call. Most Decode wall time occurred outside measured owner CPU execution. This does **not** identify whether GPU work, other native threads, blocking or preemption caused it, and does not justify raising priority or increasing decoder depth.

## Scope and limitations

- Received, decoded and freshly presented cadence in the idle scene was approximately 30 FPS, with no reported packet loss or queue-full windows. This is not a demanding 60-FPS gameplay benchmark.
- Sessions could differ in cloud conditions and were not identical bitstream replays. Lobby animation and news content could vary.
- A collector handover interrupted continuous log coverage; only received timing/histogram pairs were counted. Rollback staging occurred near the end of the repeated B run. Small differences are therefore not claimed as causal gains.
- Portable checks, ASan/UBSan, async ThreadSanitizer and native OpenOrbis build passed. Native 1080p, saved sign-in and clean session shutdown were observed. Audio PCM was nonzero; physical listening, game text entry and candidate 720p hardware tests were not repeated. Existing 720p fixtures passed.

No experimental code was retained in the active source. The restored package hash matched the previously tested 0.1.10 package, and the catalog opened without another sign-in. The public stable release remains 0.1.9; see [0.1.10 validation](../PERFORMANCE-0.1.10.md) for the retained optimization. Private logs, captures, account data and console configuration are excluded from this report.
