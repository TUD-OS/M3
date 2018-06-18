// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include <inttypes.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "traceplayer.h"
#include "exceptions.h"
#include "clock.h"

#include "platform.h"

/*
 * *************************************************************************
 */

extern "C" {
    int BENCH_evaluate(void);
}

/*
 * *************************************************************************
 */

class MeasuringTracePlayer: public TracePlayer {

  public:
    enum FlushMode {
        None = 0,
        Last = 1,
        Interval = 2,
        All = 3
    };
    enum FlushType {
        Sync = 0,
        Checkpoint = 1
    };

    MeasuringTracePlayer(std::string const &rootDir) : TracePlayer(rootDir.c_str()) { }

    virtual int play(Trace *trace, FlushMode mode, FlushType type, int num_iterations,
                     bool keep_time) {

        clock.start();

        // play trace
        for (int i = 0; i < num_iterations; i++) {
            //cout << "replay trace: iteration " << i + 1 << " of " << num_iterations << endl;
            TracePlayer::play(trace, true, true, keep_time, mode == Interval);

            // sync/checkpoint FS, if requested
            if (mode == None)
                continue;
            if (mode == Last && i + 1 != num_iterations)
                continue;
            if (type == Sync)
                Platform::sync_fs();
            else
                Platform::checkpoint_fs();
        }

        clock.stop();
        return 0;
    };

    virtual void report(std::string name) {

        cout << "'" << name << "' " << "Execution time in us: " << clock.duration(Clock::MicroSeconds) << endl;
        cout << "'" << name << "' " << "Execution time in ms: " << clock.duration(Clock::MilliSeconds) << endl;
        cout << "'" << name << "' " << "Execution time in  s: " << clock.duration(Clock::Seconds) << endl;
    };

  protected:
    Clock clock;
};

/*
 * *************************************************************************
 */

int main(int argc, char **argv) {

    try {
        Platform::init(argc, argv, "");

        // defaults
        MeasuringTracePlayer::FlushMode flush_mode = MeasuringTracePlayer::None;
        MeasuringTracePlayer::FlushType flush_type = MeasuringTracePlayer::Sync;
        long num_iterations = 1;
        bool drop_caches = false;
        bool keep_time   = false;
        bool do_revert   = false;
        std::string root_dir;
        std::string trace_name = "N/A";

        // get user-provided number of iterations, if supplied
        for (int i = 1; i < argc; i++) {
            //cout << "argv[" << i << "]=" << argv[i] << endl;
            if (i + 1 < argc && strcmp(argv[i], "--rootdir") == 0) {
                root_dir = argv[i + 1];
            }
            if (i + 1 < argc && strcmp(argv[i], "--name") == 0) {
                trace_name = argv[i + 1];
            }
            if (i + 1 < argc && strcmp(argv[i], "--iterations") == 0) {
                num_iterations = strtol(argv[i + 1], nullptr, 10);
                if (num_iterations >= 1)
                    continue;
                cout << "Invalid number of iterations!" << endl;
                throw IllegalArgumentException();
            }
            if (i + 1 < argc && strcmp(argv[i], "--flush-mode") == 0) {
                if (strstr(argv[i + 1], "sync") != 0)
                    flush_type = MeasuringTracePlayer::Sync;
                else if (strstr(argv[i + 1], "checkpoint") != 0)
                    flush_type = MeasuringTracePlayer::Checkpoint;
                if (strstr(argv[i + 1], "all") != 0)
                    flush_mode = MeasuringTracePlayer::All;
                if (strstr(argv[i + 1], "interval") != 0)
                    flush_mode = MeasuringTracePlayer::Interval;
                else if (strstr(argv[i + 1], "last") != 0)
                    flush_mode = MeasuringTracePlayer::Last;
            }
            if (strcmp(argv[i], "--cold-caches") == 0)
                drop_caches = true;
            if (strcmp(argv[i], "--keep-time") == 0)
                keep_time = true;
            if (strcmp(argv[i], "--revert") == 0)
                do_revert = true;
        }

        // playback / revert to init-trace contents
        MeasuringTracePlayer player(root_dir);
        if (do_revert) {
            // TODO player.revert();
        }
        else {
            // print parameters for reference
            char const * const sync_mode_str[3] = { "none", "last", "all" };
            char const * const sync_type_str[2] = { "sync", "checkpoint" };

            Trace *trace = Traces::get(trace_name.c_str());
            if(!trace) {
                cerr << "Trace '" << trace_name << "' does not exist.";
                return 1;
            }

            printf("VPFS trace_bench '%s' started [n=%ld,keeptime=%s,coldcaches=%s,%s=%s]\n",
                   trace_name.c_str(), num_iterations,
                   keep_time   ? "yes" : "no",
                   drop_caches ? "yes" : "no",
                   sync_type_str[flush_type],
                   sync_mode_str[flush_mode]);

            // drop all caches in both VPFS server and block server
            if (drop_caches)
                Platform::drop_caches();

            player.play(trace, flush_mode, flush_type, num_iterations, keep_time);
            player.report(trace_name);
            printf("VPFS trace_bench benchmark terminated\n");
        }

        // done
        Platform::shutdown();
    }

    catch (Exception &e) {
        e.complain();
        return 1;
    }

    return 0;
}
