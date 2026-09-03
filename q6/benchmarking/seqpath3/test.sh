#!/bin/bash
#SBATCH -w "node01"
##SBATCH -w "node04" # NODES 1 and 4 are the only ones without mca errors
#SBATCH --nodes=1
#SBATCH --time=02:00:00
#SBATCH --mail-type=END,FAIL
##SBATCH --mail-user=yash.khator@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q6/benchmarking/seqpath3

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
mpicxx -O2 -std=c++17 $1
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# create input
echo "creating input file"
printf "" > in

# number of vertices
v=8
printf "$v\n" >> in

# adjacency list
printf "1 1\n" >> in
printf "2 0 2\n" >> in
printf "2 1 3\n" >> in
printf "2 2 4\n" >> in
printf "2 3 5\n" >> in
printf "2 4 6\n" >> in
printf "2 5 7\n" >> in
printf "1 6" >> in

mpirun a.out
