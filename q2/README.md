# Q2 — Parallel Matrix Multiplication (MPI)

Computes $C = A \times B$ for $A$ ($m \times n$) and $B$ ($n \times p$) using
a master-worker MPI model. Rank 0 reads the input and distributes work; it
does not compute any part of the product itself.

## Approach

Matrix multiplication is decomposed into $n$ independent column-row (outer
product) pairs: $C = \sum_{k=0}^{n-1} A[:,k] \otimes B[k,:]$. Since each pair
contributes a full, independent $m \times p$ partial matrix, the master hands
out pairs to workers with no inter-worker communication needed:

1. Master reads $A$ (stored column-major) and $B$ (stored row-major) from
   the input file `in`.
2. The $n$ pairs are split as evenly as possible across the `P - 1` workers
   (`MPI_Scatter` for counts, `MPI_Scatterv` for the actual columns/rows),
   with any remainder pairs given one extra to the first few workers.
3. Each worker computes the outer product for every pair it received and
   accumulates them into one local partial matrix.
4. Workers reduce their partial matrices back to the master with
   `MPI_Reduce`/`MPI_SUM` to get the final $C$.

See `report/report.tex` (Section "q2") for the full theory writeup,
including a complexity analysis and discussion of the benchmark plots.

## Files

| File | Purpose |
|---|---|
| `Q2_MPI.cpp` | The MPI implementation (master + worker logic). |
| `input.sh` | Generates a random small matrix pair (dims 1-10) into `in`. |
| `fixed-input.sh` | Writes a fixed 3x3 * 3x3 test case into `in`. |
| `test.sh` | SLURM script: compiles, generates a random small input, runs once with `np=3`. |
| `test_a_to_f.sh` | SLURM script: runs 6 small hand-crafted correctness tests (Tests A-F: square, tall, wide, single-pair, scalar-output cases) at `np = 2, 3, 5, 9`, writing to `output/output_{a..f}.txt`. |
| `test_g1000.sh` / `test_g2500.sh` / `test_g5000.sh` | SLURM scripts: generate a random $N \times N$ matrix pair ($N=1000/2500/5000$) and benchmark it at `np = 2, 3, 5, 9`, writing to `output/output_g{N}.txt`. |
| `verify_correctness.sh` | Correctness check: runs a handful of hand/PDF-verified matrix pairs through the MPI program at `np = 2, 3, 5, 9` and diffs the printed product matrix against hardcoded, known-correct expected values. |
| `in` | Current input file consumed by `a.out` (`m n p` header line, then $A$ row-major, then $B$ row-major). |
| `a.out` | Compiled binary (from `mpicxx Q2_MPI.cpp`). |
| `output/` | Logged results from the `test_*.sh` scripts (correctness + timing). |
| `plots/` | Runtime/speedup/efficiency/overhead plots generated from `output/`, used in `report/`. |

## Compilation

```bash
mpicxx -O2 -std=c++17 Q2_MPI.cpp
```

On the cluster, load MPI first: `module load openmpi/4.1.5`.

## Execution

Input format (all on top of file `in`):

```
m n p
<A: m*n values, row-major>
<B: n*p values, row-major>
```

```bash
mpirun -np <P> --mca pml ob1 --mca osc ^ucx a.out
```

`P` is the total number of processes (rank 0 = master, ranks `1..P-1` =
workers, so `P >= 2`). Rank 0 prints the resulting matrix $C$ (suppressed
for dimensions >= 1000, to keep output logs readable) along with a
computation-time log.

## Running the test scripts on the cluster

```bash
sbatch test.sh            # quick smoke test, np=3, random small input
sbatch test_a_to_f.sh     # 6 correctness tests x np={2,3,5,9}
sbatch test_g1000.sh      # 1000x1000 benchmark x np={2,3,5,9}
sbatch test_g2500.sh      # 2500x2500 benchmark x np={2,3,5,9}
sbatch test_g5000.sh      # 5000x5000 benchmark x np={2,3,5,9}
```

Each script compiles `Q2_MPI.cpp` itself and writes its results under
`output/`. Check results with:

```bash
cat output/output_a.txt   # ...through output_f.txt, output_g1000.txt, etc.
```

## Correctness verification

```bash
./verify_correctness.sh
```

This prints `PASS`/`FAIL` per (test case, `np`) pair and a final
`Results: X passed, Y failed` summary (24 checks total: 6 test cases x 4
process counts).

### Running on the cluster

`mpicxx`/`mpirun` aren't on `PATH` until the OpenMPI module is loaded, and
`mpirun` shouldn't be run directly on the login node, so submit it as a
batch job instead of running it in a shell directly (the script itself
loads `openmpi/4.1.5` when the `module` command is available, so it works
either way once submitted):

```bash
sbatch --job-name=q2-verify --ntasks=9 --nodes=1 --mem-per-cpu=4G --time=00:10:00 \
  --output=%j.log --error=%j.err \
  --wrap="./verify_correctness.sh"
```

`--ntasks=9` covers the largest `np=9` case the script runs. Check the
result in `%j.log`.

## Benchmarking notes

Speedup/efficiency are computed against the `np=2` (1 worker) run as the
sequential baseline, since with exactly one worker the program does all the
same work in a single sequential pass. Key findings (full detail in
`report/report.tex`):

- For the small hand-crafted matrices (Tests A-F), actual computation takes
  microseconds; wall-clock time is dominated by MPI process startup, so more
  workers gives no real speedup.
- For the larger random matrices (1000/2500/5000), computation time drops
  noticeably with more workers, but speedup is sub-linear due to
  scatter/reduce overhead and uneven splits when `n` isn't divisible by `P`.
