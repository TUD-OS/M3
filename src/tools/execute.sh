#!/bin/bash

usage() {
    echo "Usage: $1 <script> [--debug=<prog>]" 1>&2
    exit 1
}

if [ "$1" = "-h" ] || [ "$1" = "--help" ] || [ "$1" = "-?" ]; then
    usage $0
fi

t2pcip="$M3_SSH_PREFIX"th
t2pcthip="$M3_SSH_PREFIX"thshell
build=build/$M3_TARGET-$M3_BUILD
bindir=$build/bin

. config.ini

if [ $# -lt 1 ]; then
    usage $0
fi
script=$1
shift

debug=""
for p in $@; do
    case $p in
        --debug=*)
            debug=${p#--debug=}
            ;;
    esac
done

if [ "$M3_FS" = "" ]; then
    M3_FS="default.img"
fi
export M3_FS

error() {
    echo $1 1>&2
    exit 1
}

generate_lines() {
    if [[ "`head -n 1 $1`" == \#!* ]]; then
        # workaround for bash: it executes the while-loop in a subprocess
        $1 | (
            while read line || [ -n "$line" ]; do
                echo $line
            done
        )
    else
        while read line || [ -n "$line" ]; do
            echo $line
        done <$1
    fi
}

generate_kargs() {
    c=0
    generate_lines $1 | ( while read line; do
            i=0
            for a in $line; do
                if [ $c -eq 1 ]; then
                    if [ $i -eq 0 ]; then
                        echo -n $a
                    else
                        echo -n ",$a"
                    fi
                elif [ $c -gt 1 ]; then
                    if [ $i -eq 0 ]; then
                        echo -n ",--,$a"
                    else
                        echo -n ",$a"
                    fi
                fi
                i=$((i + 1))
            done
            c=$((c + 1))
        done
    )
}

remove_kernel_args() {
    for word in $1; do
        case "$word" in
            daemon)
                ;;
            requires=*)
                ;;
            core=*)
                ;;

            *)
                echo -n "$word "
                ;;
        esac
    done
}

build_params_host() {
    generate_lines $1 | while read line; do
        echo -n "$bindir/$line -- "
    done
}

build_params_gem5() {
    kargs=`generate_kargs $1 | tr ',' ' '`

    if [ ! -z $M3_CORES ]; then
        maxcores=$M3_CORES
    else
        maxcores=`grep '#define MAX_CORES' src/include/m3/arch/gem5/Config.h | awk '{print $3 }'`
    fi

    c=0
    cmd=`generate_lines $1 | ( while read line; do
            if [ $c -eq 0 ]; then
                echo -n $bindir/$line $kargs,
            else
                echo -n $bindir/$(remove_kernel_args "$line"),
            fi
            c=$((c + 1))
        done

        while [ $c -lt $maxcores ]; do
            echo -n $bindir/idle,
            c=$((c + 1))
        done
    )`

    params=`mktemp`
    echo -n "--outdir=run --debug-file=gem5.log --debug-flags=$M3_GEM5_DBG" >> $params
    echo -n " hw/gem5/configs/example/dtu-fs.py --cpu-type $M3_GEM5_CPU --num-pes=$maxcores" >> $params
    echo -n " --cpu-clock=1GHz --sys-clock=1GHz --caches" >> $params
    echo -n " --cmd \"$cmd\" --init_mem $build/$M3_FS --debug=$debug" >> $params
    #echo -n " --watch-pe=0 --watch-start=0x111500 --watch-end=0x111600" >> $params

    export M5_PATH=$build
    if [ "$DBG_GEM5" != "" ]; then
        tmp=`mktemp`
        echo "b main" >> $tmp
        echo -n "run " >> $tmp
        cat $params >> $tmp
        echo >> $tmp
        gdb --tui hw/gem5/build/X86/gem5.debug --command=$tmp
        rm $tmp
    else
        xargs -a $params hw/gem5/build/X86/gem5.opt | tee run/log.txt
    fi

    rm $params
}

build_params_t3_sim() {
    kargs=`generate_kargs $1`

    c=0
    if [[ "$kargs" =~ "m3fs" ]]; then
        echo -n " -mem.initial_value_file=$build/mem/$M3_FS.mem -noc.noc_enable=1 "
    else
        echo -n " -mem -noc.noc_enable=1 "
    fi
    generate_lines $1 | ( while read line; do
            if [[ "$line" =~ "core=fft" ]]; then
                corename="-fftpe_core"
            else
                corename="-pe_core"
            fi
            args=""
            i=0
            for a in $line; do
                if [ $i -eq 0 ]; then
                    # SimDebugSynchronized=true means that all cores run in a synchronized fashion.
                    # this does also speed up debugging a lot because the other cores don't run at
                    # full speed while one core is run step by step.
                    echo -n " $corename.SimTargetProgram=$bindir/$a -$corename.SimDebugSynchronized=true"
                    if [ -z "$M3_NOTRACE" ]; then
                        echo -n " -$corename.SimClients=\"trace --level 6 ../../../run/core$c.log\""
                    fi
                    if [ "$a" = "$debug" ]; then
                        echo -n " -$corename.SimDebug=true -$corename.SimDebugStartingPort=1234"
                    else
                        echo -n " -$corename.SimDebug=false"
                    fi
                else
                    if [ "$args" = "" ]; then
                        args=$a
                    else
                        args="$args,$a"
                    fi
                fi
                i=$((i + 1))
            done
            if [ $c -gt 0 ]; then
                args=$(remove_kernel_args "$args")
                echo -n " -$corename.SimTargetArgs=$args"
            else
                echo -n " -$corename.SimTargetArgs=$kargs"
            fi
            c=$((c + 1))
        done

        if [ ! -z $M3_CORES ]; then
            maxcores=$M3_CORES
        else
            maxcores=`grep '#define MAX_CORES' src/include/m3/arch/t3/Config.h | awk '{print $3 }'`
        fi
        while [ $c -lt $maxcores ]; do
            echo -n " -pe_core.SimTargetProgram=$bindir/idle --pe_core.SimDebugSynchronized=true"
            if [ -z "$M3_NOTRACE" ]; then
                echo -n " --pe_core.SimClients=\"trace --level 6 ../../../run/core$c.log\""
            fi
            if [ "$debug" = "idle" ]; then
                echo -n " --pe_core.SimDebug=true --pe_core.SimDebugStartingPort=1234"
                debug=""
            else
                echo -n " --pe_core.SimDebug=false"
            fi
            echo -n " --pe_core.SimTargetArgs="
            c=$((c + 1))
        done
    )
}

build_params_t2_chip() {
    progs=""
    args=""
    kargs=""
    i=0

    if [ "$M3_CM" != "" ]; then
        cmcore="$M3_CM core=cm"
    else
        cmcore="idle core=cm"
    fi

    args=`generate_lines $1 | ( while read line; do
            c=0
            if [ $i -eq 1 ]; then
                kargs="$kargs ++ $cmcore ++"
            elif [ $i -gt 1 ]; then
                kargs="$kargs ++"
            fi
            for x in $line; do
                if [ $c -eq 0 ]; then
                    progs="$progs $x.mem"
                    if [ $i -gt 0 ]; then
                        args="$args $x.mem"
                    fi
                    kargs="$kargs $x"
                elif [ $i -gt 0 ]; then
                    args="$args $x"
                    kargs="$kargs $x"
                fi
                c=$((c + 1))
            done
            args=$(remove_kernel_args "$args")
            args="$args --"
            i=$((i + 1))
        done
        ( cd $build/mem && scp $progs $t2pcip:thtest )
        ( cd src/tools && scp chip.py $t2pcip:thtest )
        echo $kargs $args
    )`

    # ignore ^C here to pass that to the remote-side
    trap "" INT

    # generate memory layout ini-file and copy it to t2pc
    temp=`mktemp`
    $build/src/tools/consts2ini/consts2ini > $temp
    # before transferring it, make sure it is readable for everybody
    chmod +r $temp
    scp $temp $t2pcip:thtest/memlayout.ini

    # profiler on CM or APP?
    if [ "`grep CCOUNT_CM $temp | cut -d ' ' -f 3`" = "1" ]; then
        profargs="profiler_cm.mem -"
    elif [ "$M3_CM" != "" ]; then
        base=`echo $M3_CM | sed 's/\(\S*\).*/\1/g'`
        scp $build/mem/$base.mem $t2pcip:thtest
        profargs="$base.mem profiler.mem"
    else
        profargs="- profiler.mem"
    fi

    scp $build/mem/idle.mem $build/mem/profiler.mem $build/mem/profiler_cm.mem $t2pcip:thtest

    if [[ "$args" =~ "m3fs" ]]; then
        tar -C $build/mem -cf - $M3_FS.mem | gzip > $temp
        scp $temp $t2pcip:thtest/$M3_FS.mem.tar.gz
        ssh -t $t2pcip "chmod ugo+rw thtest/$M3_FS.mem.tar.gz"
        ssh -t $t2pcthip "cd thtest && source ../tomahawk_shell/setup.sh && rm -f *.txt; " \
            "echo ./chip.py $M3_FS.mem $profargs log.txt $args > run.sh && chmod +x run.sh; " \
            "tar xfz $M3_FS.mem.tar.gz && ./run.sh && " \
            "tar cfz $M3_FS.mem.out.tar.gz $M3_FS.mem.out"
        scp $t2pcip:thtest/$M3_FS.mem.out.tar.gz $build
        ( cd $build && tar xfz $M3_FS.mem.out.tar.gz &&
            mv $M3_FS.mem.out $M3_FS.out &&
            rm $M3_FS.mem.out.tar.gz )
    else
        ssh -t $t2pcthip "cd thtest && source ../tomahawk_shell/setup.sh && rm -f *.txt; " \
            "echo ./chip.py - $profargs log.txt $args > run.sh && chmod +x run.sh; " \
            "./run.sh"
    fi
    scp $t2pcip:thtest/log.txt $t2pcip:thtest/trace.txt run

    rm $temp
}

if [[ "$script" == *.cfg ]]; then
    if [ "$M3_TARGET" = "host" ]; then
        if [ "$M3_VALGRIND" != "" ]; then
            valgrind $M3_VALGRIND `build_params_host $script`
        else
            `build_params_host $script`
        fi
    elif [ "$M3_TARGET" = "t2" ]; then
        build_params_t2_chip $script
    elif [ "$M3_TARGET" = "t3" ]; then
        build=`readlink -f $build`
        bindir=`readlink -f $bindir`
        tmp=`mktemp`
        build_params_t3_sim $script > $tmp
        cd hw/th/XTSC
        echo -n "Params: "
        cat $tmp
        xargs ./t3-sim < $tmp
        rm $tmp
    elif [ "$M3_TARGET" = "gem5" ]; then
        build_params_gem5 $script
    else
        echo "Unknown target '$M3_TARGET'"
    fi
else
    $script
fi

if [ -f $build/$M3_FS.out ]; then
    $build/src/tools/m3fsck/m3fsck $build/$M3_FS.out && echo "FS image '$build/$M3_FS.out' is valid"
fi
