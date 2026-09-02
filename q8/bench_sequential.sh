#!/bin/bash
# Local sequential benchmark: times ./sequential across the same input sizes
# used in bench_mpi.sh, on the same fixed dataset config (K/S/seed), so the
# two logs are directly comparable. Results go to output/sequential_benchmark.log.
#
# usage: ./bench_sequential.sh

set -e

echo "Compiling sequential program..."
g++ -O2 -std=c++17 sequential.cpp q8_common.cpp -o sequential
echo "Compilation successful."
echo ""

mkdir -p output
RESULTS=output/sequential_benchmark.log
printf "" > "$RESULTS"

# must stay identical to bench_mpi.sh's K/S/SEED/SIZES so both logs are
# comparable point-for-point.
K=10
S=100
SEED=42

SIZES=(100 1000 10000 100000 1000000)

IN=bench_input.txt

for N in "${SIZES[@]}"; do
    echo "=== N=$N: generating input ==="
    python3 generate_dataset.py --n "$N" --k "$K" --s "$S" --seed "$SEED" --out "$IN"

    echo "N=$N K=$K S=$S seed=$SEED" >> "$RESULTS"

    { time ./sequential < "$IN" > /dev/null; } >> "$RESULTS" 2>&1
    echo "" >> "$RESULTS"

    echo "=== N=$N: deleting input ==="
    rm -f "$IN"
    echo ""
done

echo "========================================="
echo "Benchmark complete. Results in $RESULTS"
echo "========================================="
