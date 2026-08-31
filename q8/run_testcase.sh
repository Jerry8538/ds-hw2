#!/bin/bash
#SBATCH --job-name=q8-run
#SBATCH --ntasks=9
#SBATCH --nodes=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00:30:00
#SBATCH --output=%j.log
#SBATCH --error=%j.err
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=tarun.rajai@students.iiit.ac.in
#SBATCH -D /home/cs3401.49/ds-hw2/q8

# usage: sbatch run_testcase.sh [testcase-name]
# testcase-name is a file under testcases/ without the .txt extension.
# defaults to test_sample_input (N=10, small enough to verify by hand).
# %j.log ends up containing nothing but the program's required output -
# everything else (module load, compiler errors) goes to %j.err instead.

module load openmpi/4.1.5 >&2

TESTCASE=${1:-test_sample_input}
IN="testcases/${TESTCASE}.txt"

if [ ! -f "$IN" ]; then
    echo "Input file not found: $IN" >&2
    exit 1
fi

mpicxx -O2 -std=c++17 mpi.cpp q8_common.cpp -o mpi_q8 >&2
if [ $? -ne 0 ]; then
    exit 1
fi

mpirun -np $SLURM_NTASKS --mca pml ob1 --mca osc ^ucx ./mpi_q8 < "$IN"
