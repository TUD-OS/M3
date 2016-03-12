#!/usr/bin/env php
<?php
include_once 'symbols.php';

if($argc != 2)
    die("Usage: $argv[0] <trace>; with the output of nm -SC in stdin");

$funcs = get_funcs_from_stdin();

$f = @fopen($argv[1], "r");
if(!$f)
    die("Unable to open " . $argv[1]);

while(($line = fgets($f)) !== false) {
    if($line[0] == '[') {
        $l = preg_replace_callback(
            '/^\[ 0x([a-f0-9]+)/',
            function($m) {
                global $funcs;
                $addr = hexdec($m[1]);
                $func = funcname($funcs, $addr);
                if($func === null)
                    return sprintf("[ <Unknown>: %08x ", $addr);
                return sprintf('[ %-35s+0x%x ',$func[2],$addr - $func[0]);
            },
            $line
        );
        echo $l;
    }
    else
        echo $line;
}
if(!feof($f))
    die("Something went wrong");
fclose($f);
?>