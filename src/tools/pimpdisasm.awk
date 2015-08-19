BEGIN {
	IDMA_CFG_START	= 0x60600000
	IDMA_CFG_END	= 0x60620000

	# find bounds of .rodata section
	rodataStart = 0
	rodataEnd = 0
	cmd = "xt-objdump -hw " EXEC
	while((cmd | getline d) > 0) {
		if(match(d, /\.rodata\s+\S+\s+([[:xdigit:]]+)\s+\S+\s+([[:xdigit:]]+)/, m)) {
			rodataStart = strtonum("0x" m[1])
			rodataEnd = rodataStart + strtonum("0x" m[2])
			break
		}
	}
	close(cmd)
}

{
	# l32r from stext?
	if(match($0, /l32r\s*a[[:digit:]]+,\s*([[:xdigit:]]+)\s*<_stext.*$/, m)) {
		# determine the loaded value
		stextval = "??"
		addr = strtonum("0x" m[1])
		cmd = "xt-objdump -s --section=.text --start-address=" addr " --stop-address=" (addr + 4) " " EXEC
		while((cmd | getline d) > 0) {
			if(match(d, /^ [[:xdigit:]]+ ([[:xdigit:]]{2})([[:xdigit:]]{2})([[:xdigit:]]{2})([[:xdigit:]]{2})/, l)) {
				stextval = l[4] l[3] l[2] l[1]
				break
			}
		}
		close(cmd)

		# try to find out what it is
		value = "??"
		if(stextval != "??") {
			stextaddr = strtonum("0x" stextval)
			# in .rodata section?
			if(stextaddr >= rodataStart && stextaddr < rodataEnd) {
				# find the referenced string, if it is any
				off = stextaddr - rodataStart
				soff = sprintf("%x", off)
				cmd = "readelf -p .rodata " EXEC
				while((cmd | getline d) > 0) {
					if(d ~ "^\\s*\\[\\s*" soff "\\]") {
						match(d, /^\s*\[\s*\S*\]\s*(.*)$/, m)
						value = "\"" m[1] "\""
						break
					}
				}
				close(cmd)
			}
			# some iDMA address?
			else if(stextaddr >= IDMA_CFG_START && stextaddr <= IDMA_CFG_END) {
				cmd = "./build/" ENVIRON["M3_TARGET"] "-" ENVIRON["M3_MACHINE"] "-" ENVIRON["M3_BUILD"] \
					"/tools/decodeidma/decodeidma 0x" sprintf("%x", stextaddr)
				cmd | getline value
				close(cmd)
			}
			else {
				# find the referenced symbol
				cmd = "xt-nm -nC " EXEC
				while((cmd | getline d) > 0) {
					if(d ~ "^" stextval) {
						match(d, /^[[:xdigit:]]+ . (\S+)/, name)
						value = name[1]
						break
					}
				}
				close(cmd)
			}
		}

		# annotate the line
		print $0 " // " stextval "(" value ")"
	}
	else
		print $0
}
