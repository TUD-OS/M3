#!/usr/bin/env php
<?php
function usage() {
    die("Usage: (trace|functime) <binary>...\n"
        . "With the gem5 log in stdin, which should have Exec and ExecPC enabled.\n");
}

function bchexdec($hex) {
    if(strlen($hex) == 1)
        return hexdec($hex);
    else {
        $remain = substr($hex, 0, -1);
        $last = substr($hex, -1);
        return bcadd(bcmul(16, bchexdec($remain)), hexdec($last));
    }
}

function addr2func($addr) {
    global $funcs;
    static $last = null;
    if($last !== null && bccomp($addr,$last[0]) >= 0 && bccomp($addr,$last[0] + $last[1]) <= 0)
        return $last;

    foreach($funcs as $f) {
        if(bccomp($addr,$f[0]) >= 0 && bccomp($addr,$f[0] + $f[1]) <= 0) {
            $last = $f;
            return $f;
        }
    }
    return false;
}

function funcname($addr) {
    $f = addr2func($addr);
    if($f !== false) {
        return sprintf("\033[1m%s\033[0m @ %s+0x%x",
            basename($f[3]),strtok($f[2],"("),bcsub($addr,$f[0]));
    }
    return sprintf("<Unknown>: %08x", $addr);
}

function func_compare($a, $b) {
    return $a[0] < $b[0];
}

if($argc < 2)
    usage();
$mode = $argv[1];
if($mode != 'trace' && $mode != 'functime')
    usage();

$funcs = array();
for($i = 2; $i < $argc; $i++) {
    $f = popen("nm -SC " . escapeshellcmd($argv[$i]), "r");
    if(!$f)
        die("Unable to open " . $argv[$i]);
    while(($line = fgets($f)) !== false) {
        if(preg_match('/^([a-f0-9]+) ([a-f0-9]+) (t|T) ([A-Za-z0-9_:\.\~ ]+)/', $line, $match))
            $funcs[] = array(bchexdec($match[1]), bchexdec($match[2]), $match[4], $argv[$i]);
    }
    pclose($f);
}

usort($funcs,'func_compare');

$f = @fopen("php://stdin", "r");
if(!$f)
    die("Unable to open stdin");

if($mode == 'trace') {
    while(($line = fgets($f)) !== false) {
        if(preg_match('/(\d+): (pe\d+\.cpu) (\S+) : 0x([a-f0-9]+)\s*@\s*.+?\s*:\s*(.*)$/', $line, $match)) {
            $func = funcname(bchexdec($match[4]));
            printf("%7s: %s: %s : %s\n", $match[1], $match[2], $func, $match[5]);
        }
        else
            echo $line;
    }
}
else {
    $pes = array();
    while(($line = fgets($f)) !== false) {
        if(preg_match('/(\d+): (pe(\d+)\.cpu) (\S+) : 0x([a-f0-9]+)/', $line, $match)) {
            $addr = bchexdec($match[5]);
            $func = addr2func($addr);
            if($func === false)
                $func = array($addr, $addr, sprintf('%08x', $addr), '<Unknown>');
            $pe = $match[3];

            if(!isset($pes[$pe]) || $pes[$pe][0][2] != $func[2]) {
                if(isset($pes[$pe])) {
                    printf("%9s: %s: [%9u ticks] \033[1m%10.10s\033[0m @ %s+0x%x\n",
                        $match[1], $match[2], bcsub($match[1], $pes[$pe][1]),
                        basename($pes[$pe][0][3]), $pes[$pe][0][2],
                        bcsub($pes[$pe][2], $pes[$pe][0][0]));
                }
                $pes[$pe] = array($func, $match[1], $addr);
            }
        }
        else if(preg_match('/^(\d+): (.*)$/', $line, $match))
            printf("%9s: %s\n", $match[1], $match[2]);
    }
}

if(!feof($f))
    die("Something went wrong");
fclose($f);
?>
