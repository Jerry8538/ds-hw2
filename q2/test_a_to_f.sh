#!/bin/bash
#SBATCH --job-name=mpi-test-a-to-f
#SBATCH --ntasks=9
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q2

# Load necessary modules
module load openmpi/4.1.5

echo "========================================="
echo "SLURM Job ID: $SLURM_JOB_ID"
echo "Allocated nodes: $SLURM_NNODES"
echo "Total tasks: $SLURM_NTASKS"
echo "Node list: $SLURM_NODELIST"
echo "========================================="
echo ""

# Compile MPI program
echo "Compiling MPI program..."
mpicxx -O2 -std=c++17 Q2_MPI.cpp
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi
echo "Compilation successful."
echo ""

# Create output directory
mkdir -p output

# Process counts to test
PROC_COUNTS=(2 3 5 9)

# Helper: run mpirun with a given np and append result to output file
run_test() {
    local np=$1
    local outfile=$2

    echo "--- np=$np ---" >> "$outfile"
    { time mpirun -np $np --mca pml ob1 --mca osc ^ucx a.out; } >> "$outfile" 2>&1
    echo "" >> "$outfile"
}

# Helper: read the current 'in' file and pretty-print matrices A and B
print_matrices() {
    local outfile=$1
    awk '
        NR==1 { m=$1; n=$2; p=$3
            printf "Matrix A (%dx%d):\n", m, n }
        NR==2 { col=0
            for (i=1; i<=m*n; i++) {
                printf "%s ", $i; col++
                if (col==n) { printf "\n"; col=0 }
            }
            printf "\nMatrix B (%dx%d):\n", n, p }
        NR==3 { col=0
            for (i=1; i<=n*p; i++) {
                printf "%s ", $i; col++
                if (col==p) { printf "\n"; col=0 }
            }
            printf "\n"
        }
    ' in >> "$outfile"
}

# ─── Test A ───────────────────────────────────────────────────────────────────
echo "Running Test A..."
# clear in
printf "" > in
# Dimensions
printf "3 2 3\n" >> in
# Matrix A
printf "1 2 0 3 -1 4\n" >> in
# Matrix B
printf "2 3 4 1 0 -1\n" >> in

OUT=output/output_a.txt
printf "" > "$OUT"
echo "========== Test A: 3x2 * 2x3 ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test A done."
echo ""

# ─── Test B ───────────────────────────────────────────────────────────────────
echo "Running Test B..."
# clear in
printf "" > in
# Dimensions
printf "2 3 2\n" >> in
# Matrix A
printf "1 0 2 3 1 0\n" >> in
# Matrix B
printf "1 2 0 1 2 0\n" >> in

OUT=output/output_b.txt
printf "" > "$OUT"
echo "========== Test B: 2x3 * 3x2 ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test B done."
echo ""

# ─── Test C ───────────────────────────────────────────────────────────────────
echo "Running Test C (tall matrix: 100x2 * 2x3)..."
# clear in
printf "" > in
# Dimensions: m=100, n=2, p=3
printf "100 2 3\n" >> in
# 1st matrix A (100x2) -> 200 elements of '1'
for i in {1..200}; do printf "1 " >> in; done
printf "\n" >> in
# 2nd matrix B (2x3) -> 6 elements of '2'
for i in {1..6}; do printf "2 " >> in; done
printf "\n" >> in

OUT=output/output_c.txt
printf "" > "$OUT"
echo "========== Test C: 100x2 * 2x3 (tall matrix) ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test C done."
echo ""

# ─── Test D ───────────────────────────────────────────────────────────────────
echo "Running Test D (wide matrix: 3x100 * 100x2)..."
# clear in
printf "" > in
# Dimensions: m=3, n=100, p=2
printf "3 100 2\n" >> in
# 1st matrix A (3x100) -> 300 elements of '1'
for i in {1..300}; do printf "1 " >> in; done
printf "\n" >> in
# 2nd matrix B (100x2) -> 200 elements of '2'
for i in {1..200}; do printf "2 " >> in; done
printf "\n" >> in

OUT=output/output_d.txt
printf "" > "$OUT"
echo "========== Test D: 3x100 * 100x2 (wide matrix) ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test D done."
echo ""

# ─── Test E ───────────────────────────────────────────────────────────────────
echo "Running Test E (single column-row pair: 3x1 * 1x3)..."
# clear in
printf "" > in
# Dimensions: m=3, n=1, p=3
printf "3 1 3\n" >> in
# 1st matrix A (3x1)
printf "1 2 3\n" >> in
# 2nd matrix B (1x3)
printf "4 5 6\n" >> in

OUT=output/output_e.txt
printf "" > "$OUT"
echo "========== Test E: 3x1 * 1x3 (single column-row pair) ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test E done."
echo ""

# ─── Test F ───────────────────────────────────────────────────────────────────
echo "Running Test F (scalar output: 1x5 * 5x1)..."
# clear in
printf "" > in
# Dimensions: m=1, n=5, p=1
printf "1 5 1\n" >> in
# 1st matrix A (1x5)
printf "2 4 6 8 10\n" >> in
# 2nd matrix B (5x1)
printf "1 3 5 7 9\n" >> in

OUT=output/output_f.txt
printf "" > "$OUT"
echo "========== Test F: 1x5 * 5x1 (scalar output) ==========" >> "$OUT"
echo "" >> "$OUT"
print_matrices "$OUT"
for np in "${PROC_COUNTS[@]}"; do
    run_test $np "$OUT"
done
echo "Test F done."
echo ""

echo "========================================="
echo "All tests complete. Results in output/ directory."
echo "========================================="
