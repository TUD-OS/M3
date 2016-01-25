M³
==

This is the official repository of M³: **m**icrokernel-based syste**m** for heterogeneous
**m**anycores [1]. M³ is intended for platforms with very heterogeneous cores and in particular,
simple accelerators not able to run an OS kernel (e.g., DSPs, FPGAs, accelerators for specific
workloads, etc., but also general purpose cores like ARM and x86 cores). The goal is to provide
spatial isolation and access to operating-system services on all cores, i.e., treat all cores as
*first-class citizens*. This is achieved by abstracting the heterogeneity of the cores via a common
hardware component per core, called *data transfer unit* (DTU). The DTU offers all features that are
required to treat a core as first-class citizen. The most important features are secure message
passing and remote memory access.

To support very heterogeneous platforms, M³ runs the kernel on a dedicated core and lets the
applications run on the other cores. That is, no applications are running on the kernel core and no
kernel is running on the application cores. To control an application, a kernel controls its DTU
remotely, i.e., controls the communication capabilities, because remote resources (other cores,
external memories, etc.) can only be accessed via the DTU.

Supported Platforms:
--------------------

Currently, M³ runs on the following platforms:

- Tomahawk 2 [2], a silicon chip containing 9 Xtensa cores that can be used by M³, each a small
  scratchpad memory (about 64 KiB) instead of a cache. Due to the small scratchpad memory and the
  missing memory protection, traditional operating-systems like Linux are not supported.
- The next generation of Tomahawk, currently only available as a SystemC-based simulator.
- Linux, by using it as a virtual machine to simulate the DTU and the principal behaviour of the
  envisioned platform.
- gem5. We have added a DTU model to it to make M³ run on gem5.

Note that, for both Tomahawk platforms, the Xtensa toolchain and specific core configurations are
required, which are not freely available. M³ uses a private git submodule at `hw/th` for it, which
can just stay empty if gem5 or Linux are used.

Getting Started:
----------------

### Preparations:

#### Tomahawk

The cross-compiler for Xtensa needs to be build first. This is described in
`cross/README`. Furthermore, the submodule in `hw/th` needs to be pulled in:

    $ git submodule init
    $ GIT_SSH_COMMAND="ssh -l <yourname>" git submodule update hw/th

It is assumed, that you have already installed both the 4.0.2 and 5.0.5 version of the Xtensa
toolchain (into the same directory). Finally, you need to copy the `config.ini.sample` to
`config.ini` and change it accordingly.

#### gem5

The submodule in `hw/gem5` needs to be pulled in and built:

    $ git submodule init
    $ git submodule update hw/gem5
    $ cd hw/gem5
    $ scons build/X86/gem5.opt build/X86/gem5.debug

### Building:

Before you build M³, you should choose your target platform and the build-mode by exporting the
corresponding environment variables. For example:

    $ export M3_BUILD=release M3_TARGET=gem5

Now, M³ can be built by using the script `b`:

    $ ./b

### Running:

On all platforms, scenarios can be run by starting the desired boot script in the directory `boot`,
e.g.:

    $ ./b run boot/hello.cfg

Note that this command ensures that everything is up to date as well. For more information, run

    $ ./b -h

References:
-----------

[1] Nils Asmussen, Marcus Völp, Benedikt Nöthen, Hermann Härtig, and Gerhard Fettweis. *M3: A
Hardware/Operating-System Co-Design to Tame Heterogeneous Manycores*. To appear in the proceedings
of the Twenty-first International Conference on Architectural Support for Programming Languages and
Operating Systems, April 2016.

[2] Oliver Arnold, Emil Matus, Benedikt Nöthen, Markus Winter, Torsten Limberg, and Gerhard
Fettweis. *Tomahawk: Parallelism and Heterogeneity in Communications Signal Processing MPSoCs*. ACM
Transactions on Embedded Computing Systems, 13(3s):107:1–107:24, March 2014.
