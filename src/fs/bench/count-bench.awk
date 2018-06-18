BEGIN {
    sum = 0
}

/^D/ {
    sum += 1
}

END {
    printf("%d\n", sum)
}
