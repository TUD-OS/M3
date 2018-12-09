/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

//! Contains types to simplify profiling

use core::fmt;
use col::Vec;
use time;
use util;

/// A container for the measured execution times
pub struct Results {
    times: Vec<time::Time>,
}

impl Results {
    /// Creates an empty result container for the given number of runs
    pub fn new(runs: usize) -> Self {
        Results {
            times: Vec::with_capacity(runs),
        }
    }

    /// Returns the number of runs
    pub fn runs(&self) -> usize {
        self.times.len()
    }

    /// Returns the arithmetic mean of the runtimes
    pub fn avg(&self) -> time::Time {
        let mut sum = 0;
        for t in &self.times {
            sum += t;
        }
        sum / (self.times.len() as time::Time)
    }

    /// Returns the standard deviation of the runtimes
    pub fn stddev(&self) -> f32 {
        let mut sum = 0;
        let average = self.avg();
        for t in &self.times {
            let val = if *t < average {
                average - t
            }
            else {
                t - average
            };
            sum += val * val;
        }
        util::sqrt((sum as f32) / (self.times.len() as f32))
    }

    fn push(&mut self, time: time::Time) {
        self.times.push(time);
    }
}

impl fmt::Display for Results {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} cycles/iter (+/- {} with {} runs)", self.avg(), self.stddev(), self.runs())
    }
}

/// Allows to measure execution times
///
/// # Examples
///
/// Simple usage:
///
/// ```
/// use base::profile;
///
/// let mut prof = profile::Profiler::new();
/// println!("{}", prof.run_with_id(|| /* my benchmark */, 0));
/// ```
///
/// Advanced usage:
///
/// ```
/// use base::profile;
///
/// #[derive(Default)]
/// struct Tester();
///
/// impl profile::Runner for Tester {
///     fn run(&mut self) {
///         // my benchmark
///     }
///     fn post(&mut self) {
///         // my cleanup action
///     }
/// }
///
/// let mut prof = profile::Profiler::new().repeats(10).warmup(2);
/// println!("{}", prof.runner_with_id(&mut Tester::default(), 0));
/// ```
pub struct Profiler {
    repeats: u64,
    warmup: u64,
}

/// A runner is used to run the benchmarks and allows to perform pre- and post-actions.
pub trait Runner {
    /// Is executed before the benchmark
    fn pre(&mut self) {
    }

    /// Executes the benchmark
    fn run(&mut self);

    /// Is executed after the benchmark
    fn post(&mut self) {
    }
}

impl Profiler {
    /// Creates a default profiler with 100 runs and 10 warmup runs
    pub fn new() -> Self {
        Profiler {
            repeats: 100,
            warmup: 10,
        }
    }

    /// Sets the number of runs to `repeats`
    pub fn repeats(mut self, repeats: u64) -> Self {
        self.repeats = repeats;
        self
    }

    /// Sets the number of warmup runs to `warmup`
    pub fn warmup(mut self, warmup: u64) -> Self {
        self.warmup = warmup;
        self
    }

    /// Runs `func` as benchmark and returns the result
    #[inline(always)]
    pub fn run<F: FnMut()>(&mut self, func: F) -> Results {
        self.run_with_id(func, 0)
    }

    /// Runs `func` as benchmark using the given id when taking timestamps and returns the result
    ///
    /// The id is used for `time::start` and `time::stop`, which allows to identify this benchmark
    /// in the gem5 log.
    #[inline(always)]
    pub fn run_with_id<F: FnMut()>(&mut self, mut func: F, id: usize) -> Results {
        let mut res = Results::new((self.warmup + self.repeats) as usize);
        for i in 0..self.warmup + self.repeats {
            let start = time::start(id);
            func();
            let end = time::stop(id);

            if i >= self.warmup {
                res.push(end - start);
            }
        }
        res
    }

    /// Runs the given runner as benchmark using the given id when taking timestamps and returns the
    /// result
    ///
    /// The id is used for `time::start` and `time::stop`, which allows to identify this benchmark
    /// in the gem5 log.
    #[inline(always)]
    pub fn runner_with_id<R: Runner>(&mut self, runner: &mut R, id: usize) -> Results {
        let mut res = Results::new((self.warmup + self.repeats) as usize);
        for i in 0..self.warmup + self.repeats {
            runner.pre();

            let start = time::start(id);
            runner.run();
            let end = time::stop(id);

            runner.post();

            if i >= self.warmup {
                res.push(end - start);
            }
        }
        res
    }
}
