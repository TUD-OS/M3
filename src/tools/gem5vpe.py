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
cur_pe = ""

while True:
    line = sys.stdin.readline()
    if not line:
        break

    old_vpe = cur_vpe

    if "VPE_ID" in line:
        m = re.match('.*(pe[0-9]+\.).*DTU\[VPE_ID\s*\]: 0x([0-9a-f]+).*', line)
        if m:
            next_vpe = int(m[2], 16)
            next_pe = m[1]
            if cur_vpe != id or (cur_vpe == id and next_pe == cur_pe):
                cur_vpe = next_vpe
                cur_pe = next_pe

    if old_vpe != cur_vpe:
        print("------ Context Switch from %d to %d on %s ------" % (old_vpe, cur_vpe, cur_pe))
    if "PRINT: " in line or (cur_vpe == id and cur_pe in line):
        print(line.rstrip())
