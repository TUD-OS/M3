#!/bin/sh

if [ $# -ne 1 ]; then
    echo "Usage: $0 <log>" 1>&2
    exit 1
fi

log=$1
starttsc="fff1"
stoptsc="fff2"

awk '
/DMA-DEBUG-MESSAGE:/ {
    match($4, /^([[:digit:]]+)\.[[:digit:]]+\/[[:digit:]]+:$/, m)
    time = m[1]
    id = substr($7,7,4)
    if(substr($7,3,4) == "'$starttsc'") {
        #print "STRT:", id, ":", time
        start[id] = time
    }
    else if(substr($7,3,4) == "'$stoptsc'") {
        #print "STOP:", id, ":", time
        print "TIME:", id, ":", (strtonum(time) - strtonum(start[id])), "cycles"
    }
}
' $log
