#!/bin/bash
#SBATCH --job-name=mpi-test
#SBATCH --ntasks=3
#SBATCH --nodes=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=benchmark_%j.log
#SBATCH --error=benchmark_%j.err
#SBATCH --partition=debug
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=yash.khator@students.iiit.ac.in

# Load necessary modules
module load hpcx-2.7.0/hpcx-ompi

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

# create test input
bash input.sh

mpirun -n 3 a.out
