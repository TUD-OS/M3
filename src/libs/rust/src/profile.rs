use core::fmt;
use collections::Vec;
use time;
use util;

pub struct Results {
    times: Vec<time::Time>,
}

impl Results {
    pub fn new(runs: usize) -> Self {
        Results {
            times: Vec::with_capacity(runs),
        }
    }

    fn push(&mut self, time: time::Time) {
        self.times.push(time);
    }

    pub fn runs(&self) -> usize {
        self.times.len()
    }

    pub fn avg(&self) -> time::Time {
        let mut sum = 0;
        for t in &self.times {
            sum += t;
        }
        sum / (self.times.len() as time::Time)
    }

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
}

impl fmt::Display for Results {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} cycles/iter (+/- {} with {} runs)", self.avg(), self.stddev(), self.runs())
    }
}

pub struct Profiler {
    repeats: u64,
    warmup: u64,
}

impl Profiler {
    pub fn new() -> Self {
        Profiler {
            repeats: 100,
            warmup: 10,
        }
    }

    pub fn repeats(mut self, repeats: u64) -> Self {
        self.repeats = repeats;
        self
    }

    pub fn warmup(mut self, warmup: u64) -> Self {
        self.warmup = warmup;
        self
    }

    pub fn run<F: FnMut()>(&mut self, func: F) -> Results {
        self.run_with_id(func, 0)
    }

    pub fn run_with_id<F: FnMut()>(&mut self, mut func: F, id: u64) -> Results {
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
}
