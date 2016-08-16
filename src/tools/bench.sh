#!/bin/sh

if [ $# -ne 1 ] && [ $# -ne 2 ]; then
    echo "Usage: $0 <log> [<warmup>]" 1>&2
    exit 1
fi

log=$1
warmup=0
if [ "$2" != "" ]; then
    warmup=$2
fi
starttsc="1ff1"
stoptsc="1ff2"

awk -v warmup=$warmup '
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

/DMA-DEBUG-MESSAGE:/ {
    match($4, /^([[:digit:]]+)\.[[:digit:]]+\/[[:digit:]]+:$/, m)
    handle($7, m[1])
}

/DEBUG [[:xdigit:]]+/ {
    match($1, /^([[:digit:]]+):/, m)
    handle($4, m[1] / 1000)
}
' $log
