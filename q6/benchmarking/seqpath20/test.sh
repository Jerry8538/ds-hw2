#!/bin/bash
#SBATCH -w "node01"
#SBATCH --nodes=1
#SBATCH --time=02:00:00
#SBATCH --mail-type=END,FAIL
##SBATCH --mail-user=yash.khator@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q6/benchmarking/seqpath20

# Load necessary modules
module load openmpi/4.1.5

echo ""
echo "Workers: $((SLURM_NTASKS-1))"

mpirun $1
