#!/bin/bash
#SBATCH --job-name=mpi-g2500
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

# ─── Test G2500 ───────────────────────────────────────────────────────────────
echo "Generating input for Test G2500 (2500x2500 * 2500x2500)..."
m=2500; n=2500; p=2500

# clear in and write dimensions
printf "" > in
printf "$m $n $p\n" >> in

# Generate Matrix A (6,250,000 random elements) using awk
awk -v total=$((m * n)) 'BEGIN { srand(); for(i=0; i<total; i++) printf "%d ", int(rand()*10); print "" }' >> in

# Generate Matrix B (6,250,000 random elements) using awk
awk -v total=$((n * p)) 'BEGIN { srand(); for(i=0; i<total; i++) printf "%d ", int(rand()*10); print "" }' >> in

echo "Input generated."

OUT=output/output_g2500.txt
printf "" > "$OUT"
echo "========== Test G2500: ${m}x${n} * ${n}x${p} (random matrices) ==========" >> "$OUT"
echo "Matrix dimensions: A(${m}x${n})  *  B(${n}x${p})  =  C(${m}x${p})" >> "$OUT"
echo "" >> "$OUT"

for np in "${PROC_COUNTS[@]}"; do
    echo "Running with np=$np..."
    run_test $np "$OUT"
done

echo "========================================="
echo "Test G2500 complete. Results in output/output_g2500.txt"
echo "========================================="
