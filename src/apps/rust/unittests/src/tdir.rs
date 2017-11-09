use core::cmp;
use m3::col::{Vec,ToString};
use m3::test;
use m3::vfs::read_dir;

pub fn run(t: &mut test::Tester) {
    run_test!(t, list_dir);
}

fn list_dir() {
    // read a dir with known content
    let dirname = "/largedir";
    let mut vec = Vec::new();
    for e in assert_ok!(read_dir(dirname)) {
        vec.push(e);
    }
    assert_eq!(vec.len(), 82);

    // sort the entries; keep "." and ".." at the front
    vec.sort_unstable_by(|a, b| {
        let aname = a.file_name();
        let bname = b.file_name();
        let aspec = aname == "." || aname == "..";
        let bspec = bname == "." || bname == "..";
        match (aspec, bspec) {
            (true, true)    => aname.cmp(bname),
            (true, false)   => cmp::Ordering::Less,
            (false, true)   => cmp::Ordering::Greater,
            (false, false)  => {
                // cut off ".txt"
                let anum = aname[0..aname.len() - 4].parse::<i32>().unwrap();
                let bnum = bname[0..bname.len() - 4].parse::<i32>().unwrap();
                anum.cmp(&bnum)
            }
        }
    });

    // now check file names
    assert_eq!(vec[0].file_name(), ".");
    assert_eq!(vec[1].file_name(), "..");
    for i in 0..80 {
        assert_eq!(i.to_string() + ".txt", vec[i + 2].file_name());
    }
}
