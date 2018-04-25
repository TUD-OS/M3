#!/usr/bin/env python

import sys
import subprocess
import re
from os.path import basename

if len(sys.argv) < 2:
    print("Usage: %s <vpe_id>" % sys.argv[0])
    sys.exit(1)

id = int(sys.argv[1])
cur_vpe = -1

while True:
    line = sys.stdin.readline()
    if not line:
        break

    old_vpe = cur_vpe

    if "VPE_ID" in line:
        m = re.match('.*DTU\[VPE_ID\s*\]: 0x([0-9a-f]+).*', line)
        if m:
            cur_vpe = int(m[1], 16)

    if old_vpe != cur_vpe:
        print("------ Context Switch from %d to %d ------" % (old_vpe, cur_vpe))
    if cur_vpe == id:
        print(line.rstrip())
