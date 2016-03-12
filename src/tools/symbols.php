<?php
function func_compare($a, $b) {
    return $a[0] < $b[0];
}

function funcname($funcs, $addr) {
    foreach($funcs as $f) {
        if($addr >= $f[0] && $addr <= $f[0] + $f[1])
            return $f;
    }
    return null;
}

function get_funcs_from_stdin() {
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
    return $funcs;
}
?>