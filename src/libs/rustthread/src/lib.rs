#![feature(core_intrinsics)]
#![feature(shared, unique)]
#![feature(i128_type)]

#![no_std]

#[macro_use]
extern crate base;

use base::boxed::Box;
use base::cell::MutCell;
use base::cfg;
use base::col::{BoxList, Vec};
use base::dtu;
use base::libc;
use base::util;
use core::intrinsics;
use core::mem;
use core::ptr::Shared;

pub type Event = u64;

const MAX_MSG_SIZE: usize = 1024;

#[derive(Default)]
#[repr(C, packed)]
pub struct Regs {
    rbx: u64,
    rsp: u64,
    rbp: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
    rflags: u64,
    rdi: u64,
}

fn alloc_id() -> u32 {
    static NEXT_ID: MutCell<u32> = MutCell::new(0);
    NEXT_ID.set(*NEXT_ID + 1);
    *NEXT_ID
}

pub struct Thread {
    prev: Option<Shared<Thread>>,
    next: Option<Shared<Thread>>,
    id: u32,
    regs: Regs,
    stack: Vec<u64>,
    event: Event,
    has_msg: bool,
    msg: [u8; MAX_MSG_SIZE],
}

impl_boxitem!(Thread);

extern {
    fn thread_save(regs: *mut Regs) -> bool;
    fn thread_resume(regs: *mut Regs) -> bool;
}

impl Thread {
    fn new_main() -> Box<Self> {
        Box::new(Thread {
            prev: None,
            next: None,
            id: alloc_id(),
            regs: Regs::default(),
            stack: Vec::new(),
            event: 0,
            has_msg: false,
            msg: unsafe { intrinsics::uninit() },
        })
    }

    pub fn new(func_addr: u64, arg: u64) -> Box<Self> {
        let mut thread = Box::new(Thread {
            prev: None,
            next: None,
            id: alloc_id(),
            regs: Regs::default(),
            stack: vec![0u64; cfg::STACK_SIZE / 8],
            event: 0,
            has_msg: false,
            msg: unsafe { intrinsics::uninit() },
        });

        log!(THREAD, "Created thread {}", thread.id);

        // put argument in rdi and function to return to on the stack
        thread.regs.rdi = arg;
        let top_idx = thread.stack.len() - 2;
        thread.regs.rsp = &thread.stack[top_idx] as *const u64 as u64;
        thread.stack[top_idx] = func_addr;
        thread.regs.rbp = thread.regs.rsp;
        thread.regs.rflags = 0x200;    // enable interrupts

        thread
    }

    pub fn id(&self) -> u32 {
        self.id
    }
    pub fn fetch_msg(&mut self) -> Option<&'static dtu::Message> {
        if mem::replace(&mut self.has_msg, false) {
            unsafe {
                let head: *const dtu::Header = intrinsics::transmute(self.msg.as_ptr());
                let msg_len = (*head).length as usize;
                let fat_ptr: u128 = (head as u128) | (msg_len as u128) << 64;
                Some(intrinsics::transmute(fat_ptr))
            }
        }
        else {
            None
        }
    }

    #[inline(always)]
    unsafe fn save(&mut self) -> bool {
        thread_save(&mut self.regs)
    }
    #[inline(always)]
    unsafe fn resume(&mut self) -> bool {
        thread_resume(&mut self.regs)
    }

    fn subscribe(&mut self, event: Event) {
        assert!(self.event == 0);
        self.event = event;
    }

    fn trigger_event(&mut self, event: Event) -> bool {
        if self.event == event {
            self.event = 0;
            true
        }
        else {
            false
        }
    }

    fn set_msg(&mut self, msg: &'static dtu::Message) {
        let size = msg.header.length as usize + util::size_of::<dtu::Header>();
        self.has_msg = true;
        unsafe {
            libc::memcpy(
                self.msg.as_ptr() as *mut libc::c_void,
                msg as *const dtu::Message as *const libc::c_void,
                size
            );
        }
    }
}

pub struct ThreadManager {
    current: Option<Box<Thread>>,
    ready: BoxList<Thread>,
    block: BoxList<Thread>,
    sleep: BoxList<Thread>,
}

static TMNG: MutCell<Option<ThreadManager>> = MutCell::new(None);

pub fn init() {
    TMNG.set(Some(ThreadManager::new()));
}

impl ThreadManager {
    fn new() -> Self {
        ThreadManager {
            current: Some(Thread::new_main()),
            ready: BoxList::new(),
            block: BoxList::new(),
            sleep: BoxList::new(),
        }
    }

    pub fn get() -> &'static mut ThreadManager {
        TMNG.get_mut().as_mut().unwrap()
    }

    pub fn cur(&self) -> &Box<Thread> {
        self.current.as_ref().unwrap()
    }
    fn cur_mut(&mut self) -> &mut Box<Thread> {
        self.current.as_mut().unwrap()
    }

    pub fn thread_count(&self) -> usize {
        self.ready.len() + self.block.len() + self.sleep.len()
    }
    pub fn ready_count(&self) -> usize {
        self.ready.len()
    }
    pub fn sleeping_count(&self) -> usize {
        self.sleep.len()
    }

    pub fn fetch_msg(&mut self) -> Option<&'static dtu::Message> {
        match self.current {
            Some(ref mut t) => t.fetch_msg(),
            None            => None,
        }
    }

    pub fn add_thread(&mut self, func_addr: u64, arg: u64) {
        self.sleep.push_back(Thread::new(func_addr, arg));
    }

    pub fn alloc_event(&self) -> Event {
        static NEXT_EVENT: MutCell<Event> = MutCell::new(0);
        // if we have no other threads available, don't use events
        if self.sleeping_count() == 0 {
            0
        }
        // otherwise, use a unique number
        else {
            NEXT_EVENT.set(NEXT_EVENT.get() + 1);
            *NEXT_EVENT.get()
        }
    }

    pub fn wait_for(&mut self, event: Event) {
        let next = self.get_next();

        let mut cur = mem::replace(&mut self.current, Some(next)).unwrap();
        cur.subscribe(event);
        log!(THREAD, "Thread {} waits for {:#x}, switching to {}", cur.id, event, self.cur().id);

        unsafe {
            let old = Box::into_raw(cur);
            self.block.push_back(Box::from_raw(old));
            // we can't call a function between save and resume
            if !(*old).save() {
                self.cur_mut().resume();
            }
        }
    }

    pub fn try_yield(&mut self) {
        match self.ready.pop_front() {
            None        => return,
            Some(next)  => {
                let cur = mem::replace(&mut self.current, Some(next)).unwrap();
                log!(THREAD, "Yielding from {} to {}", cur.id, self.cur().id);

                unsafe {
                    let old = Box::into_raw(cur);
                    self.sleep.push_back(Box::from_raw(old));
                    if !(*old).save() {
                        self.cur_mut().resume();
                    }
                }
            },
        }
    }

    pub fn notify(&mut self, event: Event, msg: Option<&'static dtu::Message>) {
        let mut it = self.block.iter_mut();
        while let Some(t) = it.next() {
            if t.trigger_event(event) {
                if let Some(m) = msg {
                    t.set_msg(m);
                }
                log!(THREAD, "Waking up thread {} for event {:#x}", t.id, event);
                let t = it.remove();
                self.ready.push_back(t.unwrap());
            }
        }
    }

    pub fn stop(&mut self) {
        let next = self.get_next();
        let mut cur = mem::replace(&mut self.current, Some(next)).unwrap();
        log!(THREAD, "Stopping thread {}, switching to {}", cur.id, self.cur().id);

        unsafe {
            if !cur.save() {
                self.cur_mut().resume();
            }
        }
    }

    fn get_next(&mut self) -> Box<Thread> {
        if self.ready.len() > 0 {
            self.ready.pop_front().unwrap()
        }
        else {
            self.sleep.pop_front().unwrap()
        }
    }
}
