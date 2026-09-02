#!/bin/bash
#SBATCH --job-name=q8-verify-opti-try
#SBATCH --ntasks=9
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00:30:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q8

# Cluster correctness check for mpi_opti_try.cpp - same differential
# testing approach as verify_correctness.sh (diff against sequential,
# multiple process counts, every testcase), but actually run on this
# cluster's node/filesystem, since mpi_opti_try has every worker
# independently seek/read the input file concurrently - a behavior
# bench_mpi_opti_try.sh never checks (it pipes stdout to /dev/null, it
# only measures time). This is what actually confirms the speedup numbers
# in output/mpi_opti_try_benchmark.log came from a CORRECT program.
#
# usage: sbatch verify_opti_try.sh

module load openmpi/4.1.5

echo "=== Building sequential ==="
g++ -std=c++17 -O2 -Wall sequential.cpp q8_common.cpp -o sequential
if [ $? -ne 0 ]; then echo "sequential build failed"; exit 1; fi

echo "=== Building mpi_opti_try ==="
mpicxx -std=c++17 -O2 -Wall mpi_opti_try.cpp q8_common.cpp -o mpi_opti_try
if [ $? -ne 0 ]; then echo "mpi_opti_try build failed"; exit 1; fi

PASS=0
FAIL=0

echo ""
echo "=== mpi_opti_try output vs sequential output, multiple process counts ==="

TESTCASES=(testcases/test_*.txt testcases/data_100.txt testcases/data_1K.txt testcases/data_10K.txt)
PROCESS_COUNTS=(2 4 9)

for input in "${TESTCASES[@]}"; do
    ./sequential < "$input" > /tmp/q8_verify_opti_seq.out
    for p in "${PROCESS_COUNTS[@]}"; do
        mpirun -np "$p" --mca pml ob1 --mca osc ^ucx --mca btl vader,self --mca btl_vader_single_copy_mechanism none ./mpi_opti_try "$input" 2>/dev/null > /tmp/q8_verify_opti_mpi.out
        if diff -q /tmp/q8_verify_opti_seq.out /tmp/q8_verify_opti_mpi.out > /dev/null; then
            PASS=$((PASS+1))
        else
            FAIL=$((FAIL+1))
            echo "FAIL  $(basename "$input") (P=$p)"
            diff /tmp/q8_verify_opti_seq.out /tmp/q8_verify_opti_mpi.out
        fi
    done
done

echo ""
echo "=== record-count check at data_10M.txt (no diff - floating-point"
echo "    summation order legitimately differs here, see README.md) ==="
./sequential < testcases/data_10M.txt > /tmp/q8_verify_opti_seq10m.out
for p in 2 3 5 9; do
    mpirun -np "$p" --mca pml ob1 --mca osc ^ucx --mca btl vader,self --mca btl_vader_single_copy_mechanism none ./mpi_opti_try testcases/data_10M.txt 2>/dev/null > /tmp/q8_verify_opti_mpi10m.out
    seq_total=$(head -1 /tmp/q8_verify_opti_seq10m.out)
    mpi_total=$(head -1 /tmp/q8_verify_opti_mpi10m.out)
    if [ "$seq_total" == "$mpi_total" ]; then
        echo "PASS  data_10M.txt (P=$p): $mpi_total"
        PASS=$((PASS+1))
    else
        echo "FAIL  data_10M.txt (P=$p): sequential=$seq_total mpi_opti_try=$mpi_total"
        FAIL=$((FAIL+1))
    fi
done

echo ""
echo "=== Summary: $PASS passed, $FAIL failed ==="

if [ "$FAIL" -ne 0 ]; then
    echo "VERIFICATION: FAILED"
    exit 1
fi

echo "VERIFICATION: PASSED"
