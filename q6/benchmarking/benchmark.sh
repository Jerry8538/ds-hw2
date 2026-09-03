# script to manually run all benchmarks
# run for both sequential and parallel versions of the program
# test.sh needs to exist because sbatch needs to be passed a script (?)

# TODO maybe the source of the slowdown?
mem="4G"
node="node01" # this and node04 are the only ones without mca errors

module load openmpi/4.1.5

echo "compiling"
mpicxx -O2 -std=c++17 ~/ds-hw2/q6/sequential.cpp -o sequential
mpicxx -O2 -std=c++17 ~/ds-hw2/q6/parallel.cpp -o parallel

cd seqpath3
name="sequential"
sbatch --job-name=$name --ntasks=1 --mem-per-cpu=$mem --output=%x-%j.log --error=%x-%j.err test.sh sequential
name="parallel"
for cores in 2 4 8 16; do
    sbatch --job-name=$name --ntasks=$((cores+1)) --mem-per-cpu=$mem --output=%x-$cores-%j.log --error=%x-$cores-%j.err test.sh parallel
    # TODO verify the output with the sequential one
done
