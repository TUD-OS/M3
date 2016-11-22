import os, sys
import ConfigParser
import StringIO
sys.path.insert(0, 'src/tools')
import install

target = os.environ.get('M3_TARGET')
if target == 't2' or target == 't3':
    toolversion = 'RE-2014.5-linux' if target == 't3' else 'RD-2011.2-linux'

    # config (prefix it with [root] to make it usable from bash and python)
    ini_str = '[root]\n' + open('hw/th/config.ini', 'r').read()
    config = ConfigParser.RawConfigParser()
    config.readfp(StringIO.StringIO(ini_str))

    if target == 't3':
        core = 'Pe_4MB_128k_4irq'
    else:
        core = 'oi_lx4_PE_6'
    isa = 'xtensa'
    cross = 'xtensa-buildroot-linux-uclibc-'
    crossdir = Dir(config.get('root', 'buildroot')).abspath + '/host/usr/'
    crossver = '5.3.0'
    runtime = 'sim' if target == 't3' else 'min-rt'
    configpath = Dir(config.get('root', 'cfgpath'))
    xtroot = Dir(config.get('root', 'xtroot'))
    tooldir = Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/bin')
    crtdir = crossdir + '/lib/gcc/' + cross[:-1] + '/' + crossver
elif target == 'gem5':
    isa = os.environ.get('M3_ISA')
    if isa is None:
        isa = 'x86_64'
    if isa == 'arm':
        cross = 'arm-none-eabi-'
    else:
        cross = ''
    configpath = Dir('.')
else:
    # build for host by default
    isa = os.popen("uname -m").read().strip()
    target = 'host'
    cross = ''
    configpath = Dir('.')

# build basic environment
baseenv = Environment(
    CPPFLAGS = '-D__' + target + '__',
    CXXFLAGS = ' -std=c++11 -Wall -Wextra -Wsign-conversion -fdiagnostics-color=always',
    CFLAGS = ' -std=c99 -Wall -Wextra -Wsign-conversion',
    CPPPATH = ['#src/include'],
    ENV = {
        'PATH' : os.environ['PATH'],
        # required for colored outputs
        'HOME' : os.environ['HOME'],
        'TERM' : os.environ['TERM'],
    }
)

# print executed commands?
verbose = os.environ.get('M3_VERBOSE', 0)
if int(verbose) == 0:
    baseenv['INSTALLSTR']   = "[INSTALL] $TARGET"
    baseenv['ASPPCOMSTR']   = "[AS     ] $TARGET"
    baseenv['ASPPCOMSTR']   = "[ASPP   ] $TARGET"
    baseenv['CCCOMSTR']     = "[CC     ] $TARGET"
    baseenv['SHCCCOMSTR']   = "[SHCC   ] $TARGET"
    baseenv['CXXCOMSTR']    = "[CXX    ] $TARGET"
    baseenv['SHCXXCOMSTR']  = "[SHCXX  ] $TARGET"
    baseenv['LINKCOMSTR']   = "[LD     ] $TARGET"
    baseenv['SHLINKCOMSTR'] = "[SHLD   ] $TARGET"
    baseenv['ARCOMSTR']     = "[AR     ] $TARGET"
    baseenv['RANLIBCOMSTR'] = "[RANLIB ] $TARGET"
    baseenv['F90COMSTR']    = "[FC     ] $TARGET"
    baseenv['MDUMPCOMSTR']  = "[MDUMP  ] $TARGET"
    baseenv['STRIPCOMSTR']  = "[STRIP  ] $TARGET"
    baseenv['DUMPCOMSTR']   = "[DUMP   ] $TARGET"
    baseenv['MKFSCOMSTR']   = "[MKFS   ] $TARGET"
    baseenv['CPPCOMSTR']    = "[CPP    ] $TARGET"

# for host compilation
hostenv = baseenv.Clone()
hostenv.Append(
    CPPFLAGS = ' -D__tools__'
)

# for target compilation
env = baseenv.Clone()
env.Append(
    CXXFLAGS = ' -ffreestanding -fno-strict-aliasing -fno-exceptions -fno-rtti -gdwarf-2' \
        ' -fno-threadsafe-statics -fno-stack-protector',
    CPPFLAGS = ' -U_FORTIFY_SOURCE',
    CFLAGS = ' -gdwarf-2',
    ASFLAGS = ' -Wl,-W -Wall -Wextra',
    LINKFLAGS = ' -fno-exceptions -fno-rtti -Wl,--no-gc-sections -Wno-lto-type-mismatch',
)

# allow to add preprocessor flags via env variable
cppdefines = []
for flag in os.environ.get('M3_CFLAGS', '').split():
   cppdefines.append(flag)
env.Append(CPPDEFINES = cppdefines)

# add target-dependent stuff to env
if target == 't2' or target == 't3':
    env.Append(
        # align it appropriately for the DTU
        LINKFLAGS = ' -nostdlib -Wl,-z,max-page-size=8 -Wl,-z,common-page-size=8',
        CPPPATH = [
            Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/include'),
            Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/xtensa-elf/include')
        ],
        SUPDIR = Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/lib'),
        CRTDIR = Dir(crtdir)
    )
    env.Replace(ENV = {'PATH' : crossdir + '/bin:' + tooldir.abspath + ':' + os.environ['PATH']})
else:
    env.Append(CXXFLAGS = ' -fno-omit-frame-pointer')
    if target == 'gem5':
        if isa == 'x86_64':
            # disable red-zone for all applications, because we use the application's stack in rctmux's
            # IRQ handlers since applications run in privileged mode.
            env.Append(CFLAGS = ' -mno-red-zone')
            env.Append(CXXFLAGS = ' -mno-red-zone')
        else:
            env.Append(CFLAGS = ' -march=armv7-a')
            env.Append(CXXFLAGS = ' -march=armv7-a')
            env.Append(LINKFLAGS = ' -march=armv7-a')
            env.Append(ASFLAGS = ' -march=armv7-a')
        # no build-id because it confuses gem5
        env.Append(LINKFLAGS = ' -static -Wl,--build-id=none -nostdlib')
        # binaries get very large otherwise
        env.Append(LINKFLAGS = ' -Wl,-z,max-page-size=4096 -Wl,-z,common-page-size=4096')

env.Replace(CXX = cross + 'g++')
env.Replace(AS = cross + 'gcc')
env.Replace(CC = cross + 'gcc')
env.Replace(LD = cross + 'ld')
env.Replace(AR = cross + 'gcc-ar')
env.Replace(RANLIB = cross + 'gcc-ranlib')
env.Replace(FORTRAN = cross + 'gfortran')
env.Replace(F90 = cross + 'gfortran')

# add build-dependent flags (debug/release)
btype = os.environ.get('M3_BUILD', 'release')
if btype == 'debug':
    if target == 'host' or target == 'gem5':
        env.Append(CXXFLAGS = ' -O0 -g')
        env.Append(CFLAGS = ' -O0 -g')
    else:
        # use -Os here because otherwise the binaries tend to get larger than 32k
        env.Append(CXXFLAGS = ' -Os -g')
        env.Append(CFLAGS = ' -Os -g')
    env.Append(ASFLAGS = ' -g')
    hostenv.Append(CXXFLAGS = ' -O0 -g')
    hostenv.Append(CFLAGS = ' -O0 -g')
else:
    if target == 't2':
        env.Append(CXXFLAGS = ' -Os -DNDEBUG -flto')
        env.Append(CFLAGS = ' -Os -DNDEBUG -flto')
        env.Append(LINKFLAGS = ' -Os -flto')
    else:
        env.Append(CXXFLAGS = ' -O2 -DNDEBUG -flto')
        env.Append(CFLAGS = ' -O2 -DNDEBUG -flto')
        env.Append(LINKFLAGS = ' -O2 -flto')
builddir = 'build/' + target + '-' + isa + '-' + btype

if target == 't2' or target == 't3':
    archtype = 'th'
else:
    archtype = target

# add some important paths
env.Append(
    ARCH = target,
    ISA = isa,
    ARCHTYPE = archtype,
    BUILD = btype,
    CFGS = configpath,
    BUILDDIR = Dir(builddir),
    BINARYDIR = Dir(builddir + '/bin'),
    LIBDIR = Dir(builddir + '/bin'),
    MEMDIR = Dir(builddir + '/mem'),
    FSDIR = Dir(builddir + '/fsdata')
)
hostenv.Append(
    BINARYDIR = env['BINARYDIR']
)

def M3MemDump(env, target, source):
    dump = env.Command(
        target, source,
        Action(
            'xt-dumpelf --width=64 --offset=0 $SOURCE > $TARGET',
            '$MDUMPCOMSTR'
        )
    )
    env.Install('$MEMDIR', dump)

def M3FileDump(env, target, source, addr, args = ''):
    dump = env.Command(
        target, source,
        Action(
            '$BUILDDIR/src/tools/dumpfile/dumpfile $SOURCE 0x%x %s > $TARGET' % (addr, args),
            '$DUMPCOMSTR'
        )
    )
    env.Depends(dump, '$BUILDDIR/src/tools/dumpfile/dumpfile')
    env.Install('$MEMDIR', dump)

def M3Mkfs(env, target, source, blocks, inodes, blks_per_ext):
    fs = env.Command(
        target, source,
        Action(
            '$BUILDDIR/src/tools/mkm3fs/mkm3fs $TARGET $SOURCE %d %d %d' % (blocks, inodes, blks_per_ext),
            '$MKFSCOMSTR'
        )
    )
    env.Depends(fs, '$BUILDDIR/src/tools/mkm3fs/mkm3fs')
    env.Install('$BUILDDIR', fs)

def M3Strip(env, target, source):
    return env.Command(
        target, source,
        Action(
            cross + 'strip -o $TARGET $SOURCE',
            '$STRIPCOMSTR'
        )
    )

def M3CPP(env, target, source):
    env.Command(
        target, source,
        Action(
            cross + 'cpp -P $CPPFLAGS $SOURCE $TARGET',
            '$CPPCOMSTR'
        )
    )

def_ldscript = env.File('$BUILDDIR/ld-final.conf')
M3CPP(env, def_ldscript, '#src/toolchain/gem5/ld.conf')

link_addr = 0x200000

def M3Program(env, target, source, libs = [], libpaths = [], NoSup = False, tgtcore = None,
              ldscript = None, varAddr = True):
    myenv = env.Clone()

    m3libs = ['base', 'thread'] if target == 'kernel' else ['base', 'm3', 'thread']

    if myenv['ARCH'] == 't2' or myenv['ARCH'] == 't3':
        # set variables, depending on core
        if tgtcore is None:
            tgtcore = core
        runtimedir = configpath.abspath + '/' + tgtcore + '/xtensa-elf/lib/' + runtime

        sources = []
        if not NoSup:
            sources += [
                myenv['LIBDIR'].abspath + '/crti.o',
                crtdir + '/crtbegin.o',
                crtdir + '/crtend.o',
                myenv['LIBDIR'].abspath + '/crtn.o',
                myenv['LIBDIR'].abspath + '/Window.o'
            ]
            libs = ['hal', 'handlers-sim', 'gcc', 'c'] + m3libs + libs
        sources += source

        if ldscript is None:
            ldscript = File(runtimedir + '/ldscripts/elf32xtensa.x')
        myenv.Append(LINKFLAGS = ' -Wl,-T,' + ldscript.abspath)

        myenv.Append(CPPPATH = [
            '#src/include',
            Dir(configpath.abspath + '/' + tgtcore + '/xtensa-elf/arch/include'),
            Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/xtensa-elf/include'),
        ])

        prog = myenv.Program(
            target,
            sources,
            LIBS = ['handler-reset'] + libs,
            LIBPATH = [myenv['LIBDIR'], myenv['SUPDIR']] + libpaths,
            SUPDIR = Dir(configpath.abspath + '/' + tgtcore + '/xtensa-elf/arch/lib')
        )
        myenv.M3MemDump(target + '.mem', prog)
        myenv.Depends(prog, ldscript)
        myenv.Depends(prog, File(runtimedir + '/specs'))
        myenv.Depends(prog, myenv['LIBDIR'].abspath + '/libm3.a')
    elif myenv['ARCH'] == 'gem5':
        if not NoSup:
            libs = ['gcc', 'c'] + m3libs + libs
            source = [myenv['LIBDIR'].abspath + '/crt0.o'] + [source]

        if ldscript is None:
            ldscript = def_ldscript
        myenv.Append(LINKFLAGS = ' -Wl,-T,' + ldscript.abspath)

        if varAddr:
            global link_addr
            myenv.Append(LINKFLAGS = ' -Wl,--section-start=.text=' + ("0x%x" % link_addr))
            link_addr += 0x10000

        prog = myenv.Program(
            target, source,
            LIBS = libs,
            LIBPATH = [myenv['LIBDIR']] + libpaths
        )
        myenv.Depends(prog, ldscript)
    else:
        prog = myenv.Program(
            target, source,
            LIBS = m3libs + ['m3', 'pthread'] + libs,
            LIBPATH = [myenv['LIBDIR']] + libpaths
        )

    myenv.Install(myenv['BINARYDIR'], prog)
    return prog

env.AddMethod(M3MemDump)
env.AddMethod(M3FileDump)
env.AddMethod(M3Mkfs)
env.AddMethod(M3Strip)
env.AddMethod(M3CPP)
env.AddMethod(install.InstallFiles)
env.M3Program = M3Program

# always use grouping for static libraries, because they may depend on each other so that we want
# to cycle through them until all references are resolved.
env['_LIBFLAGS'] = '-Wl,--start-group ' + env['_LIBFLAGS'] + ' -Wl,--end-group'

env.SConscript('src/SConscript', exports = ['env', 'hostenv'], variant_dir = builddir, src_dir = '.', duplicate = 0)
