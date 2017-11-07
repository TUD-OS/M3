use arch::dtu::*;
use col::Vec;
use core::intrinsics;
use libc;
use util;

pub(crate) struct SocketBackend {
    sock: i32,
    localsock: Vec<i32>,
    eps: Vec<libc::sockaddr_un>,
}

impl SocketBackend {
    pub fn new() -> SocketBackend {
        let sock = unsafe { libc::socket(libc::AF_UNIX, libc::SOCK_DGRAM, 0) };
        assert!(sock != -1);

        let mut eps = vec![];
        for pe in 0..PE_COUNT {
            for ep in 0..EP_COUNT {
                let addr = format!("\0m3_ep_{}.{}\0", pe, ep);
                let mut sockaddr = libc::sockaddr_un {
                    sun_family: libc::AF_UNIX as libc::sa_family_t,
                    sun_path: [0; 108],
                };
                sockaddr.sun_path[0..addr.len()].clone_from_slice(
                    unsafe { intrinsics::transmute(addr.as_bytes()) }
                );
                eps.push(sockaddr);
            }
        }

        let pe = arch::env::data().pe_id();
        let mut localsock = vec![];
        for ep in 0..EP_COUNT {
            unsafe {
                let epsock = libc::socket(libc::AF_UNIX, libc::SOCK_DGRAM, 0);
                assert!(epsock != -1);

                assert!(libc::fcntl(epsock, libc::F_SETFD, libc::FD_CLOEXEC) == 0);
                assert!(libc::fcntl(epsock, libc::F_SETFL, libc::O_NONBLOCK) == 0);

                assert!(libc::bind(
                    epsock,
                    intrinsics::transmute(&eps[pe as usize * EP_COUNT + ep]),
                    util::size_of::<libc::sockaddr_un>() as u32
                ) == 0);

                localsock.push(epsock);
            }
        }

        SocketBackend {
            sock: sock,
            localsock: localsock,
            eps: eps,
        }
    }

    pub fn send(&self, pe: PeId, ep: EpId, buf: &thread::Buffer) {
        unsafe {
            let sock = &self.eps[pe * EP_COUNT + ep];
            let res = libc::sendto(
                self.sock,
                buf as *const thread::Buffer as *const libc::c_void,
                buf.header.length + util::size_of::<Header>(),
                0,
                sock as *const libc::sockaddr_un as *const libc::sockaddr,
                util::size_of::<libc::sockaddr_un>() as u32
            );
            assert!(res != -1);
        }
    }

    pub fn receive(&self, ep: EpId, buf: &mut thread::Buffer) -> Option<usize> {
        unsafe {
            let res = libc::recvfrom(
                self.localsock[ep],
                buf as *mut thread::Buffer as *mut libc::c_void,
                util::size_of::<thread::Buffer>(),
                0,
                ptr::null_mut(),
                ptr::null_mut(),
            );
            if res <= 0 {
                None
            }
            else {
                Some(res as usize)
            }
        }
    }
}

impl Drop for SocketBackend {
    fn drop(&mut self) {
        for ep in 0..EP_COUNT {
            unsafe { libc::close(self.localsock[ep]) };
        }
    }
}
