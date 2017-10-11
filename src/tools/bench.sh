#!/bin/sh

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo "Usage: $0 <log> <mhz> [<warmup>]" 1>&2
    exit 1
fi

log=$1
mhz=$2
warmup=0
if [ "$3" != "" ]; then
    warmup=$3
fi
starttsc="1ff1"
stoptsc="1ff2"

awk -v warmup=$warmup -v mhz=$mhz '
function handle(msg, pe, time) {
    id = substr(msg,7,4)
    idx = sprintf("%d.%s", pe, id)
    if(substr(msg,3,4) == "'$starttsc'") {
        start[idx] = time
    }
    else if(substr(msg,3,4) == "'$stoptsc'") {
        counter[idx] += 1
        if(counter[idx] > warmup)
            printf("PE%d-TIME: %04s : %d cycles\n", pe, id, strtonum(time) - strtonum(start[idx]))
    }
}

function ticksToCycles(ticks) {
    return ticks * (mhz / 1000000)
}

/DMA-DEBUG-MESSAGE:/ {
    match($4, /^([[:digit:]]+)\.[[:digit:]]+\/[[:digit:]]+:$/, m)
    handle($7, m[1])
}

/DEBUG [[:xdigit:]]+/ {
    match($1, /^([[:digit:]]+):/, time)
    match($2, /(pe|cpu)([[:digit:]]+)/, pe)
    handle($4, pe[2], ticksToCycles(time[1]))
}
' $log
