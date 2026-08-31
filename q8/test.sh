#!/bin/bash
#SBATCH --job-name=q8-test
#SBATCH --ntasks=9
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q8

# Load necessary modules
module load openmpi/4.1.5

echo "========================================="
echo "SLURM Job ID: $SLURM_JOB_ID"
echo "Allocated nodes: $SLURM_NNODES"
echo "Total tasks: $SLURM_NTASKS"
echo "Node list: $SLURM_NODELIST"
echo "========================================="
echo ""

# Compile sequential program
echo "Compiling sequential program..."
g++ -O2 -std=c++17 sequential.cpp q8_common.cpp -o sequential
if [ $? -ne 0 ]; then
    echo "Sequential compilation failed!"
    exit 1
fi

# Compile MPI program
echo "Compiling MPI program..."
mpicxx -O2 -std=c++17 mpi.cpp q8_common.cpp -o mpi_q8
if [ $? -ne 0 ]; then
    echo "MPI compilation failed!"
    exit 1
fi
echo "Compilation successful."
echo ""

# small pre-generated testcase - this job is a smoke test (does everything
# build and run correctly in this cluster's module/network environment),
# not the real benchmark - that's a separate script with larger datasets
# and multiple process counts.
IN=testcases/data_100.txt

echo "=== Running sequential ==="
{ time ./sequential < "$IN" > seq_output.txt; } > seq_time.txt 2>&1
cat seq_time.txt
echo ""

# same UCX workaround as q2/q6: UCX isn't installed on this cluster, so
# OpenMPI's default transport selection prints noisy warnings/fails to
# init. --mca pml ob1 forces the standard point-to-point protocol instead,
# and --mca osc ^ucx excludes UCX from one-sided communication selection.
echo "=== Running MPI (np=9) ==="
{ time mpirun -np 9 --mca pml ob1 --mca osc ^ucx ./mpi_q8 < "$IN" > mpi_output.txt; } > mpi_time.txt 2>&1
cat mpi_time.txt
echo ""

echo "=== Correctness check: sequential vs MPI output ==="
if diff -q seq_output.txt mpi_output.txt > /dev/null; then
    echo "PASS: MPI output matches sequential output"
else
    echo "FAIL: MPI output does not match sequential output"
    diff seq_output.txt mpi_output.txt
fi
