#!/usr/bin/env php
<?php
if($argc != 2)
    die("Usage: $argv[0] <trace>; with the output of nm -SC in stdin");

function func_compare($a, $b) {
    return $a[0] < $b[0];
}

function funcname($funcs, $addr) {
    foreach($funcs as $f) {
        if($addr >= $f[0] && $addr <= $f[0] + $f[1])
            return $f[2];
    }
    return sprintf("<Unknown>: %08x", $addr);
}

$funcs = array();
$f = @fopen('php://stdin', "r");
if(!$f)
    die("Unable to open stdin");
while(($line = fgets($f)) !== false) {
    if(preg_match('/^([a-f0-9]+) ([a-f0-9]+) (t|T) (.+)$/', $line, $match))
        $funcs[] = array(hexdec($match[1]), hexdec($match[2]), $match[4]);
}
fclose($f);

usort($funcs,'func_compare');

$f = @fopen($argv[1], "r");
if(!$f)
    die("Unable to open " . $argv[1]);

$count = 0;
$laststart = 0;
$lastaddr = 0;
$lastfunc = '';
while(($line = fgets($f)) !== false) {
    if($line[0] == '[')
        $lastaddr = hexdec(substr($line, 4, 8));
    else if(substr($line, 0, 6) == "\t\$time") {
        preg_match('/^\s*\$time\(\s*"iss",\s*"clocks",\s*(\\d+),\s*"cycle",\s*(\\d+)/', $line, $match);
        $func = funcname($funcs, $lastaddr);
        if($lastfunc != '' && $func != $lastfunc) {
            printf("@%8d: %5d: %s\n", $laststart, $count, $lastfunc);
            $laststart = $match[2];
            $count = 0;
        }
        $lastfunc = $func;
        $count += $match[1];
    }
}
if(!feof($f))
    die("Something went wrong");
fclose($f);
?>