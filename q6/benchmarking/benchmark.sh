# script to manually run all benchmarks
# run for both sequential and parallel versions of the program

# TODO maybe the source of the slowdown?
mem="4G"

cd seqpath3
name="sequential"
sbatch --job-name=$name --ntasks=1 --mem-per-cpu=$mem --output=%x-%j.log --error=%x-%j.err test.sh ~/ds-hw2/q6/sequential.cpp
name="parallel"
for cores in 2 4 8 16; do
    sbatch --job-name=$name --ntasks=$((cores+1)) --mem-per-cpu=$mem --output=%x-$cores-%j.log --error=%x-$cores-%j.err test.sh ~/ds-hw2/q6/parallel.cpp
    # TODO verify the output with the sequential one
done
