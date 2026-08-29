#!/bin/bash
#SBATCH --job-name=q6
#SBATCH -w "node01"
##SBATCH -w "node04" # NODES 1 and 4 are the only ones without mca errors
#SBATCH --ntasks=3
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
##SBATCH --mail-user=yash.khator@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q6

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
mpicxx -O2 -std=c++17 q6.cpp
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# create input
echo "creating input file"
printf "" > in

# number of vertices
let v=2**20
printf "$v\n" >> in

# adjacency list
printf "1 1\n" >> in
for i in $(seq 1 $((v-2))); do
    printf "2 $((i-1)) $((i+1))\n" >> in
done
printf "1 $((v-2))" >> in

mpirun a.out
