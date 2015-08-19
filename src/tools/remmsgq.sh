#!/bin/sh

for id in `ipcs -q | grep '^0x' | cut -f 2 -d ' '`; do
    ipcrm -q $id
done