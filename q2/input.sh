# dimensions TODO
printf "3 3 3\n" >> in

# 1st matrix
for _ in {1..3}; do
    for __ in {1..3}; do
        printf "$RANDOM " >> in
    done
done
printf "\n" >> in

# 2nd matrix
for _ in {1..3}; do
    for __ in {1..3}; do
        printf "$RANDOM " >> in
    done
done
printf "\n" >> in

printf "2\n" >> in
