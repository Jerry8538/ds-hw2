#!/bin/bash
# Correctness verification for Q2.
#
# Runs a handful of small matrix pairs (including the two worked examples
# from the assignment PDF, plus a couple of edge cases) through the MPI
# program at np = 2, 3, 5, 9 (i.e. 1, 2, 4, 8 workers) and diffs the printed
# product matrix against the pre-computed, known-correct answer for each
# case (hand/PDF-verified, hardcoded below).

cd "$(dirname "$0")"

# On the cluster, mpicxx/mpirun aren't on PATH until this module is loaded;
# locally (e.g. Homebrew MPI on macOS) the `module` command doesn't exist,
# so skip it there instead of failing.
if command -v module &>/dev/null; then
    module load openmpi/4.1.5
fi

echo "Compiling MPI implementation..."
mpicxx -O2 -std=c++17 Q2_MPI.cpp -o a.out || exit 1

PROC_COUNTS=(2 3 5 9)
PASS=0
FAIL=0

# Pulls just the numeric rows of matrix C out of Q2_MPI's stdout (drops the
# "my num_pairs: ..." debug lines and the COMPUTATION LOG block).
extract_mpi_matrix() {
    awk '/the final product matrix C:/{flag=1; next} /^===/{exit} flag' | grep -E '^[-0-9 ]+$'
}

run_case() {
    local name="$1"
    local expected="$2"   # expected matrix values, space-separated, row-major

    for np in "${PROC_COUNTS[@]}"; do
        mpirun -np "$np" --mca pml ob1 --mca osc ^ucx ./a.out > /tmp/q2_mpi_raw.txt 2>&1
        extract_mpi_matrix < /tmp/q2_mpi_raw.txt > /tmp/q2_mpi_out.txt
        got=$(tr -s ' \t\n' ' ' < /tmp/q2_mpi_out.txt | sed 's/^ //;s/ $//')

        if [ "$got" == "$expected" ]; then
            echo "PASS: $name (np=$np)"
            PASS=$((PASS + 1))
        else
            echo "FAIL: $name (np=$np)"
            echo "  expected: $expected"
            echo "  got     : $got"
            FAIL=$((FAIL + 1))
        fi
    done
}

# --- Test A: 3x2 * 2x3 (assignment PDF, Q2 Example 1) ---
printf "3 2 3\n1 2 0 3 -1 4\n2 3 4 1 0 -1\n" > in
run_case "Test A: 3x2*2x3 (PDF Example 1)" "4 3 2 3 0 -3 2 -3 -8"

# --- Test B: 2x3 * 3x2 (assignment PDF, Q2 Example 2, uneven split) ---
printf "2 3 2\n1 0 2 3 1 0\n1 2 0 1 2 0\n" > in
run_case "Test B: 2x3*3x2 (PDF Example 2)" "5 2 3 7"

# --- Test E: 3x1 * 1x3 (n=1 edge case: single column-row pair) ---
printf "3 1 3\n1 2 3\n4 5 6\n" > in
run_case "Test E: n=1 edge case 3x1*1x3" "4 5 6 8 10 12 12 15 18"

# --- Test F: 1x5 * 5x1 (m=1, p=1 edge case: scalar output) ---
printf "1 5 1\n2 4 6 8 10\n1 3 5 7 9\n" > in
run_case "Test F: m=1,p=1 edge case 1x5*5x1" "190"

# --- Test C: 100x2 * 2x3 (tall matrix, m >> n; every entry is 1*2+1*2=4) ---
{
    printf "100 2 3\n"
    for i in $(seq 1 200); do printf "1 "; done; printf "\n"
    for i in $(seq 1 6);   do printf "2 "; done; printf "\n"
} > in
expected_c=""
for i in $(seq 1 300); do expected_c="$expected_c 4"; done
run_case "Test C: 100x2*2x3 tall" "${expected_c# }"

# --- Test D: 3x100 * 100x2 (wide matrix, n >> m; every entry is 100*(1*2)=200) ---
{
    printf "3 100 2\n"
    for i in $(seq 1 300); do printf "1 "; done; printf "\n"
    for i in $(seq 1 200); do printf "2 "; done; printf "\n"
} > in
expected_d=""
for i in $(seq 1 6); do expected_d="$expected_d 200"; done
run_case "Test D: 3x100*100x2 wide" "${expected_d# }"

echo ""
echo "========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "========================================="
[ "$FAIL" -eq 0 ]
