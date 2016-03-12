#!/usr/bin/env php
<?php
include_once 'symbols.php';

if($argc != 2)
    die("Usage: $argv[0] <trace>; with the output of nm -SC in stdin");

$funcs = get_funcs_from_stdin();

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
        if($lastfunc != '' && $func[2] != $lastfunc) {
            printf("@%8d: %5d: %s\n", $laststart, $count, $lastfunc);
            $laststart = $match[2];
            $count = 0;
        }
        $lastfunc = $func[2];
        $count += $match[1];
    }
}
if(!feof($f))
    die("Something went wrong");
fclose($f);
?>