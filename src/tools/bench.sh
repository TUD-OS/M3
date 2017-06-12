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
function handle(msg, time) {
    id = substr(msg,7,4)
    if(substr(msg,3,4) == "'$starttsc'") {
        start[id] = time
    }
    else if(substr(msg,3,4) == "'$stoptsc'") {
        counter[id] += 1
        if(counter[id] > warmup)
            print "TIME:", id, ":", (strtonum(time) - strtonum(start[id])), "cycles"
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
    match($1, /^([[:digit:]]+):/, m)
    handle($4, ticksToCycles(m[1]))
}
' $log
