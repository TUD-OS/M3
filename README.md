
M³
==

This is the official repository of M³: **m**icrokernel-based syste**m** for heterogeneous **m**anycores [1, 2]. M³ is the operating system for a new system architecture that considers heterogeneous compute units (general-purpose cores with different instruction sets, DSPs, FPGAs, fixed-function accelerators, etc.) from the beginning instead of as an afterthought. The goal is to integrate all compute units (CUs) as *first-class citizens*, enabling 1) isolation and secure communication between all types of CUs, 2) direct interactions of all CUs to remove the conventional CPU from the critical path, 3) access to OS services such as file systems and network stacks for all CUs, and 4) context switching support on all CUs.

The system architecture is based on a hardware/operating system co-design with two key ideas:

1) introduce a new hardware component next to each CU used by the OS as the CUs' common interface and
2) let the OS kernel control applications remotely from a different CU.

The new hardware component is called data transfer unit (DTU). Since not all CUs can be expected to offer the architectural features that are required to run an OS kernel, M³ runs the kernel on a dedicated CU and the  applications on the remaining CUs. To control an application, a kernel controls its DTU remotely, because CU-external resources (other CUs, memories, etc.) can only be accessed via the DTU.

Supported Platforms:
--------------------

Currently, M³ runs on the following platforms:

- gem5, by adding a DTU model to gem5.
- Linux, by using Linux' primitives to simulate the behavior of the DTU and the envisioned system architecture.

Getting Started:
----------------

### Preparations for gem5:

The submodule in `hw/gem5` needs to be pulled in and built:

    $ git submodule init
    $ git submodule update hw/gem5
    $ cd hw/gem5
    $ git submodule init && git submodule update
    $ scons build/X86/gem5.opt build/X86/gem5.debug

### Building:

Before you build M³, you should choose your target platform and the build-mode by exporting the corresponding environment variables. For example:

    $ export M3_BUILD=release M3_TARGET=gem5

Now, M³ can be built by using the script `b`:

    $ ./b

### Running:

On all platforms, scenarios can be run by starting the desired boot script in the directory `boot`, e.g.:

    $ ./b run boot/hello.cfg

Note that this command ensures that everything is up to date as well. For more information, run

    $ ./b -h

References:
-----------

[1] Nils Asmussen, Michael Roitzsch, and Hermann Härtig. *M3x: Autonomous Accelerators via Context-Enabled Fast-Path Communication*. To appear in the Proceedings of the 2019 USENIX Annual Technical Conference (USENIX ATC'19).

[2] Nils Asmussen, Marcus Völp, Benedikt Nöthen, Hermann Härtig, and Gerhard Fettweis. *M3: A Hardware/Operating-System Co-Design to Tame Heterogeneous Manycores*. In Proceedings of the Twenty-first International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS'16), pages 189-203, April 2016.

