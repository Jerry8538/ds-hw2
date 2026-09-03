#!/bin/bash
#SBATCH --job-name=q8-mpi-tree-merge-bench
#SBATCH --ntasks=9
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=04:00:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q8

# Benchmarks mpi_tree_merge.cpp (the binomial-tree worker-merge variant of
# mpi.cpp - see mpi_tree_merge.cpp's header comment) with the exact same
# config/sizes/process counts as bench_mpi.sh, so
# output/mpi_tree_merge_benchmark.log is directly comparable to
# output/mpi_benchmark.log.

module load openmpi/4.1.5

echo "========================================="
echo "SLURM Job ID: $SLURM_JOB_ID"
echo "Allocated nodes: $SLURM_NNODES"
echo "Total tasks: $SLURM_NTASKS"
echo "Node list: $SLURM_NODELIST"
echo "CPUs allocated to this job (SLURM_JOB_CPUS_PER_NODE): $SLURM_JOB_CPUS_PER_NODE"
echo "CPUs visible to this shell (nproc): $(nproc)"
echo "========================================="
echo ""

echo "Compiling MPI tree-merge program..."
mpicxx -O2 -std=c++17 mpi_tree_merge.cpp q8_common.cpp -o mpi_tree_merge
if [ $? -ne 0 ]; then
    echo "MPI tree-merge compilation failed!"
    exit 1
fi
echo "Compilation successful."
echo ""

mkdir -p output
RESULTS=output/mpi_tree_merge_benchmark.log
printf "" > "$RESULTS"

# must stay identical to bench_mpi.sh's K/S/SEED/SIZES/PROC_COUNTS so the
# two logs are directly comparable.
K=10
S=100
SEED=42

# P is the TOTAL process count (rank 0 = master), so numWorkers = P-1 gives
# 1, 2, 4, 8 workers - doubling worker counts, same P values q2 benchmarks with
PROC_COUNTS=(2 3 5 9)

SIZES=(100 1000 10000 100000 1000000 20000000)

IN=bench_input_tree_merge.txt

for N in "${SIZES[@]}"; do
    echo "=== N=$N: generating input ==="
    python3 generate_dataset.py --n "$N" --k "$K" --s "$S" --seed "$SEED" --out "$IN"

    echo "N=$N K=$K S=$S seed=$SEED" >> "$RESULTS"

    for P in "${PROC_COUNTS[@]}"; do
        echo "=== N=$N, P=$P ==="
        echo "--- P=$P ---" >> "$RESULTS"
        # same vader-over-TCP workaround as bench_mpi.sh - see that script's
        # comment for why.
        { time mpirun -np $P --mca pml ob1 --mca osc ^ucx --mca btl vader,self --mca btl_vader_single_copy_mechanism none ./mpi_tree_merge < "$IN" > /dev/null; } >> "$RESULTS" 2>&1
        echo "" >> "$RESULTS"
    done

    echo "=== N=$N: deleting input ==="
    rm -f "$IN"
    echo ""
done

echo "========================================="
echo "Benchmark complete. Results in $RESULTS"
echo "========================================="
