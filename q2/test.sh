#!/bin/bash
#SBATCH --job-name=mpi-test
#SBATCH --ntasks=3
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q2

# Load necessary modules
# module load hpcx-2.7.0/hpcx-ompi TODO FUCKER DOESNT GET PAST INIT
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

# create test input
bash input.sh

# this helps us measure the time taken by our program, but this also includes the time taken for OpenMPI network setup, spawning processes across diff cores, establishing network connections and finally closing down the connections at end. All this overhead is also included in this timing
# by default the time command prints on stderr(fd 2) so we are redirecting to stdout(fd 1)
{ time mpirun a.out; } > output.txt 2>&1