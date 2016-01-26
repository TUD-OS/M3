#!/usr/bin/env php
<?php
if($argc != 1)
    die("Usage: $argv[0]; with the objdump -d in stdin");

$f = @fopen('php://stdin', "r");
if(!$f)
    die("Unable to open stdin");

$last = false;
$lastaddr = 0;
while(($line = fgets($f)) !== false) {
    if(preg_match('/^([a-f0-9]+) <([^\+>]+)>:$/', $line, $match)) {
        $addr = hexdec($match[1]);
        if($last !== false)
            printf("%08x %08x T %s\n",$last[0],$addr - $last[0],$last[1]);
        $last = array($addr, $match[2]);
        $lastaddr = $addr;
    }
    else if(preg_match('/^([a-f0-9]+):/', $line, $match))
        $lastaddr = hexdec($match[1]);
}
if($last !== false)
    printf("%08x %08x T %s\n",$last[0],$lastaddr - $last[0],$last[1]);
fclose($f);
?>