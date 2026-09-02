# Q8 — Large-Scale Weather and Environmental Data Analytics

Sequential and MPI implementations for HW2 Q8 (Real-World Applications
section). Computes total/average/min/max weather statistics, Top-K
stations, hottest/coldest measurements, the busiest 60-second interval,
and extreme-temperature event counts over a stream of weather station
measurements.

## Files

| File | Purpose |
|---|---|
| `q8_common.hpp` / `q8_common.cpp` | Shared analytics: `Measurement`/`Stats` structs, `updateStats()`, `mergingWorkerProcessStats()`, Top-K, output formatting. Used unchanged by both implementations. |
| `sequential.cpp` | Single-process entry point. Simulates a 1-master + 8-worker decomposition on one OS process (see PLAN.md). |
| `mpi.cpp` | Real MPI entry point. Rank 0 is the master; ranks `1..P-1` are workers. |
| `generate_dataset.py` | Reproducible dataset generator (fixed seed). |
| `verify_correctness.sh` | One-shot script: builds everything, diffs sequential output against hand-verified expected output, then diffs MPI output against sequential output across multiple process counts and every testcase. |
| `testcases/` | Self-generated correctness inputs (no official sample I/O exists for Q8 — confirmed absent from the assignment PDF and by TA clarification) plus larger generated datasets for benchmarking. |
| `testcases/expected/` | Hand-verified expected output for a curated set of testcases, one per required tie-break rule / edge case. |

## Compilation

```bash
g++ -std=c++17 -O2 sequential.cpp q8_common.cpp -o sequential
mpicxx -std=c++17 -O2 mpi.cpp q8_common.cpp -o mpi_q8
```

## Execution

Input format:

```
N K S
timestamp station_id temperature humidity pressure rainfall wind_speed
... (N lines)
```

`N` = number of measurements, `K` = number of Top-K stations to report,
`S` = number of distinct stations (`station_id` in `[0, S-1]`).

```bash
./sequential < input.txt > output.txt
mpirun -np <P> ./mpi_q8 < input.txt > output.txt
```

`P` is the **total** number of MPI processes (rank 0 = master, ranks
`1..P-1` = workers, so `P` must be at least 2). Only rank 0 writes the
required output to stdout.

## Dataset generation

```bash
python3 generate_dataset.py --n 1000 --k 5 --s 20 --seed 42 --out data.txt
```

Fixed seed (`42` by default) makes generation reproducible. Value ranges
and other generation assumptions are documented in the script's header
comment.

## Correctness verification

No official sample input/output exists for Q8 (checked against the
assignment PDF directly — Q4–Q6 have worked samples, Q7/Q8 do not — and
confirmed by the TA clarification thread). Correctness is instead
established two ways:

1. **Expected-output checks** — `testcases/expected/` holds hand-verified
   expected output for one input file per required tie-break rule / edge
   case (Top-K tie, hottest tie, coldest tie, busiest-interval tie,
   extreme-temperature boundary, a station's records split across
   multiple worker partitions, `K > S`, `N = 0`, plus a general sample).
   `sequential`'s actual output is diffed against these byte-for-byte.
2. **Differential testing** — with sequential now verified, it becomes
   the correctness oracle for MPI: `mpi_q8`'s output is diffed against
   `sequential`'s output across every testcase in `testcases/` (including
   `N < numWorkers` and `N` not divisible by `numWorkers`), at multiple
   process counts.

Run everything with:

```bash
./verify_correctness.sh
```

This builds `sequential` and `mpi_q8`, runs both checks above at
`P = 2, 4, 9`, and reports a final pass/fail summary (non-zero exit on
any failure).

### Known limitation: floating-point rounding at large N

`sequential`'s summation isn't a single straight linear pass — it calls
`runMasterWorkerSimulation()` with a fixed 8 logical workers, so it sums
each of 8 partitions separately and merges them, same as the real MPI
program does for actual workers. Floating-point addition isn't
associative, so summing the same values in a different number of chunks
(e.g. 1 chunk at real `P=2`, vs `sequential`'s fixed 8 chunks) can round
differently in the last decimal place after millions of additions.

In practice this is invisible at the small `testcases/` sizes
`verify_correctness.sh` uses, and `P=9` (8 real workers) always matches
`sequential` exactly, since the partition shape is identical. But at
large N (observed at N=2,000,000) sums like `TOTAL_RAINFALL` can differ
from `sequential`'s output by 1 in the last printed decimal digit at
some other `P` values (e.g. `P=2`, `P=3`) — not a correctness bug, just
expected floating-point non-associativity, and consistent with the
assignment's own note that "where floating-point accumulation order can
affect the final rounded value, use a deterministic strategy." Not
fixed here (would require compensated/Kahan summation or an
order-independent reduction); documented as a known limitation instead.

## Architecture notes

- `P` always means the *total* number of MPI processes. Rank 0 is a
  non-computing master (reads input, partitions it, distributes it,
  merges worker results, prints output); ranks `1..P-1` are workers
  (`numWorkers = P - 1`).
- Records are distributed with `MPI_Scatterv` using a balanced
  contiguous partition (same rule the sequential simulation uses).
- Each worker's local `Stats` is sent back to the master as plain
  primitive arrays (grouped by C++ type) over a handful of `MPI_Send`
  calls — not a derived MPI datatype — since at most a few workers each
  send one aggregated result, so the message count is negligible next to
  the cost of processing the underlying measurements. The `Measurement`
  datatype used for `MPI_Scatterv` *is* a derived MPI datatype, since
  millions of individual records cross that wire.
- The master reconstructs each worker's `Stats` from the received
  arrays and merges it with the same `mergingWorkerProcessStats()` the
  sequential version uses — no separate MPI-side merge logic.

## Future improvements (not part of the graded deliverable)

Benchmarking showed the master's serial `cin >>` parse of the entire
input (before `MPI_Scatterv` even starts) dominates wall-clock time at
every process count — `mpi.cpp`'s real time is nearly flat across
`P=2,3,5` at large N, since that fixed serial parse caps how much
speedup more workers can give (Amdahl's law).

`mpi_opti_try.cpp` explores the fix: instead of the master reading+parsing
everything then scattering it, every worker independently opens the input
file and reads only its own byte-range directly off disk in parallel
(input distribution is free-form per the TA clarification), so only the
small aggregated `Stats` results cross MPI. This requires passing the
input as a real file path argument (`./mpi_opti_try <file>`) rather than
piping via stdin, since MPI doesn't reliably forward stdin to every rank.

Locally verified (not yet cluster-benchmarked): diffed against
`sequential` across the full `testcases/` set at `P = 2, 3, 4, 5, 9`
(80/80 match byte-for-byte), plus `TOTAL_MEASUREMENTS` checked to match
exactly at `data_10M.txt` for every tested P (confirms no records are
lost or duplicated by the byte-range boundary logic — the only
divergence there is the same floating-point summation-order rounding
already documented above, not a bug). `bench_mpi_opti_try.sh` runs the
same benchmark config as `bench_mpi.sh` (same K/S/seed/sizes/process
counts) against this variant, logging to
`output/mpi_opti_try_benchmark.log` for direct comparison — not yet run
on the cluster.

## Status

Benchmarking (execution time across input sizes/process counts,
speedup/efficiency plots, and the communication-vs-computation analysis)
is not yet included in this README — that section will be added once
benchmarking is complete.
