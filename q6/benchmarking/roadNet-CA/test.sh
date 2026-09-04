#!/bin/bash
#SBATCH -w "node01"
#SBATCH --nodes=1
#SBATCH --time=02:00:00
#SBATCH --mail-type=END,FAIL
##SBATCH --mail-user=yash.khator@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q6/benchmarking/roadNet-CA

# Load necessary modules
module load openmpi/4.1.5

echo "========================================="
echo "SLURM Job ID: $SLURM_JOB_ID"
echo "Allocated nodes: $SLURM_NNODES"
echo "Total tasks: $SLURM_NTASKS"
echo "Node list: $SLURM_NODELIST"
echo "========================================="
echo ""

mpirun $1
