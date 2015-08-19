#!/usr/bin/env php
<?php
if($argc != 2)
    die("Usage: $argv[0] <trace>; with the objdump in stdin");

$f = @fopen($argv[1], "r");
if(!$f)
    die("Unable to open " . $argv[1]);

$times = array();
$counts = array();
$misses = array();
$lastaddr = 0;
while(($line = fgets($f)) !== false) {
    if($line[0] == '[')
        $lastaddr = substr($line, 4, 8);
    else if(substr($line, 0, 6) == "\t\$time") {
        preg_match('/^\s*\$time\(\s*"iss",\s*"clocks",\s*(\\d+),/', $line, $match);
        if(!isset($times[$lastaddr])) {
            $times[$lastaddr] = 0;
            $counts[$lastaddr] = 0;
        }
        $times[$lastaddr] += $match[1];
        $counts[$lastaddr]++;
    }
    else if(preg_match('/\s*\$miss\(\s*"(.*?)"/', $line, $match)) {
        if(!isset($misses[$match[1]][$lastaddr]))
            $misses[$match[1]][$lastaddr] = 0;
        $misses[$match[1]][$lastaddr]++;
    }
}
if(!feof($f))
    die("Something went wrong");
fclose($f);

$total = 0;
foreach($times as $a => $t)
    $total += $t;

$f = @fopen('php://stdin', "r");
if(!$f)
    die("Unable to open " . $argv[1]);
while(($line = fgets($f)) !== false) {
    if(preg_match('/^\s*([a-f0-9]+):/', $line, $match)) {
        $addr = sprintf("%08s", $match[1]);
        // , T%02d,%02d
        @printf("[%5d cyc, %4.1f%%, %3dx, \$%02d,%02d] %s",
            $times[$addr], 100 * ($times[$addr] / $total), $counts[$addr],
            $misses["icache"][$addr], $misses["dcache"][$addr],
            //$misses["itlb"][$addr], $misses["dtlb"][$addr],
            $line);
    }
    else
        echo $line;
}
if(!feof($f))
    die("Something went wrong");
fclose($f);
?>