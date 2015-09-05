#!/usr/bin/env php
<?php
class SendOp {
    public $op;
    public $tsc;
    public $fire;
    public $local_mod;
    public $local_ep;
    public $local_addr;
    public $size;
    public $msgsize;
    public $remote_mod;
    public $remote_addr;
    public $remote_ep;
    public $label;
    public $credits;
    public $header_enabled;
    public $reply_enabled;
    public $reply_label;
    public $reply_crd;
    public $reply_ep;
};

define('OP_READ',      0);
define('OP_WRITE',     1);
define('OP_RECV',      2);

define('ST_DEF',       0);
define('ST_SEND',      1);

$next_is_reply = array();
$state = ST_DEF;
$send = NULL;

$f = fopen("php://stdin", "r") or exit("Unable to open stdin");
while($line = fgets($f)) {
    switch($state) {
        case ST_DEF:
            if(strstr($line,'---------------------------------') !== false) {
                $state = ST_SEND;
                $send = new SendOp;
            }
            else if(preg_match('/iDma_pe(\d+)\s+-\s+(\d+).*?last packet of Msg received! '
                    . '\(slotId: (\d+); from chip\/module: \d+\/(\d+)\)/',$line,$m)) {
                printf("[\e[1m%d <- %d\e[0m @ %6d] over EP %d\n",
                    $m[1] + 1,$m[4],$m[2],$m[3]);
            }
            else if(preg_match('/iDma_pe(\d+).*?REPLY_CAP_RESP_CMD: ADDR:/',$line,$m))
                $next_is_reply[$m[1] + 1] = true;
            else if(preg_match('/iDma_pe(\d+)\s+-\s+(\d+).*?DMA-DEBUG-MESSAGE: (0x[a-f0-9]+);/',$line,$m))
                printf("[\e[1m%d TSC \e[0m @ %6d] %s\n",$m[1] + 1,$m[2],$m[3]);
            break;

        case ST_SEND:
            if(strstr($line,'---------------------------------') !== false) {
                if($send->op == OP_READ || $send->op == OP_WRITE) {
                    if($send->remote_addr >= 0xe1000000) {
                        $remote_ep = ($send->remote_addr - 0xe1000000) / 0x800;
                        printf("[\e[1m%d %s %d\e[0m @ %6d] over EP %d of [0x%08x:0x%04x] to EP %d\n",
                            $send->local_mod,
                            $next_is_reply[$send->local_mod] ? ">>" : "->",
                            $send->remote_mod,$send->tsc,
                            $send->local_ep,$send->local_addr,$send->size,
                            $remote_ep);
                        printf("  header: %d, lbl: 0x%08x, reply: (%d, lbl: 0x%08x, EP: %d, crd: %d)\n",
                            $send->header_enabled,$send->label,
                            $send->reply_enabled,$send->reply_label,$send->reply_ep,$send->reply_crd);
                    }
                    else {
                        printf("[\e[1m%d %s %d\e[0m @ %6d] over EP %d of [0x%08x:0x%04x] at 0x%08x (crd: %d)\n",
                            $send->local_mod,
                            $send->op == OP_READ ? "rd" : "wr",
                            $send->remote_mod,$send->tsc,
                            $send->local_ep,$send->local_addr,$send->size,
                            $send->remote_addr,$send->credits);
                    }
                }
                else {
                    printf("[\e[1m%d recv\e[0m @ %6d] with EP %d at [0x%08x:0x%04x:0x%04x]\n",
                        $send->local_mod,$send->tsc,
                        $send->local_ep,$send->local_addr,$send->size,$send->msgsize);
                }

                $next_is_reply[$send->local_mod] = false;
                $state = ST_DEF;
            }
            else {
                if(preg_match('/dmaSlotConfig: fire: (\d+); Mode: DMA-([A-Z]+)/',$line,$m))
                    $send->op = $m[1] == 1 ? ($m[2] == 'READ' ? OP_WRITE : OP_READ) : OP_RECV;
                else if(preg_match('/iDma_pe(\d+)\s+-\s+(\d+).*?SLOT-Label: 0x([[:xdigit:]]+);/',$line,$m)) {
                    $send->local_mod = $m[1] + 1;
                    if(!isset($next_is_reply[$send->local_mod]))
                        $next_is_reply[$send->local_mod] = false;
                    $send->tsc = $m[2];
                    $send->label = intval($m[3], 16);
                }
                else if(preg_match('/iDMA Config of slotId: (\d+);/',$line,$m))
                    $send->local_ep = $m[1];
                else if(preg_match('/LOCAL_CFG_ADDRESS_FIFO_CMD: ADDR: 0x([[:xdigit:]]+)/',$line,$m))
                    $send->local_addr = intval($m[1], 16);
                else if($send->op != OP_RECV && preg_match('/transfer size: (\d+)/',$line,$m))
                    $send->size = intval($m[1]) * 8;
                else if($send->op == OP_RECV && preg_match('/Fifo-Size: (\d+)/',$line,$m))
                    $send->size = intval($m[1]);
                else if($send->op == OP_RECV && preg_match('/Addr-Inc:\s*(\d+)/',$line,$m))
                    $send->msgsize = intval($m[1], 16);
                else if(preg_match('/HeaderConfig: enabled: (\d+); replyEnable: (\d+)/',$line,$m)) {
                    $send->header_enabled = $m[1] == 1;
                    $send->reply_enabled = $m[2] == 1;
                }
                else if(preg_match('/HeaderConfig: replyLabel: 0x([[:xdigit:]]+)/',$line,$m))
                    $send->reply_label = intval($m[1],16);
                else if(preg_match('/HeaderConfig: replySlotId: (\d+)/',$line,$m))
                    $send->reply_ep = $m[1];
                else if(preg_match('/HeaderConfig: replyCredits: (\d+)/',$line,$m))
                    $send->reply_crd = $m[1];
                else if(preg_match('/ModuleId: 0x([[:xdigit:]]+)/',$line,$m))
                    $send->remote_mod = intval($m[1],16);
                else if(preg_match('/targetId: 0x0; addr: 0x([[:xdigit:]]+)/',$line,$m))
                    $send->remote_addr = intval($m[1],16);
                else if(preg_match('/Credits: (\d+)/',$line,$m))
                    $send->credits = $m[1];
            }
            break;
    }
}
fclose($f);
?>