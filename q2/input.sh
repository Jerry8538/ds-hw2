# clear in
echo "" > in

# dimensions
m=$(($RANDOM%10+1))
n=$(($RANDOM%10+1))
p=$(($RANDOM%10+1))
printf "$m $n $p\n" >> in

# 1st matrix
for _ in $(seq 1 $m); do
    for __ in $(seq 1 $n); do
        printf "$RANDOM " >> in
    done
done
printf "\n" >> in

# 2nd matrix
for _ in $(seq 1 $n); do
    for __ in $(seq 1 $p); do
        printf "$RANDOM " >> in
    done
done
printf "\n" >> in

printf "3\n" >> in
#printf "$(($RANDOM%8+1))\n" >> in
