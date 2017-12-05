use core::fmt;

use m3::boxed::Box;
use m3::col::{BoxRef, BoxList};
use m3::test;

struct TestItem {
    data: u32,
    prev: Option<BoxRef<TestItem>>,
    next: Option<BoxRef<TestItem>>,
}

impl_boxitem!(TestItem);

impl PartialEq for TestItem {
    fn eq(&self, other: &TestItem) -> bool {
        self.data == other.data
    }
}

impl fmt::Debug for TestItem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "data={}", self.data)
    }
}

impl TestItem {
    pub fn new(data: u32) -> Self {
        TestItem {
            data: data,
            prev: None,
            next: None,
        }
    }
}

pub fn run(t: &mut test::Tester) {
    run_test!(t, create);
    run_test!(t, basics);
    run_test!(t, iter);
    run_test!(t, iter_remove);
    run_test!(t, push_back);
    run_test!(t, push_front);
}

fn gen_list(items: &[u32]) -> BoxList<TestItem> {
    let mut l: BoxList<TestItem> = BoxList::new();
    for i in items {
        l.push_back(Box::new(TestItem::new(*i)));
    }
    l
}

fn create() {
    let l: BoxList<TestItem> = BoxList::new();
    assert_eq!(l.len(), 0);
    assert_eq!(l.iter().next(), None);
}

fn basics() {
    let mut l = gen_list(&[23, 42, 57]);

    assert_eq!(l.len(), 3);
    assert_eq!(l.front().unwrap().data, 23);
    assert_eq!(l.back().unwrap().data, 57);

    assert_eq!(l.front_mut().unwrap().data, 23);
    assert_eq!(l.back_mut().unwrap().data, 57);
}

fn iter() {
    let mut l: BoxList<TestItem> = gen_list(&[23, 42, 57]);

    {
        let mut it = l.iter_mut();
        let e = it.next();
        assert_eq!(e.as_ref().unwrap().data, 23);
        e.map(|v| (*v).data = 32);

        let e = it.next();
        assert_eq!(e.as_ref().unwrap().data, 42);
        e.map(|v| (*v).data = 24);

        let e = it.next();
        assert_eq!(e.as_ref().unwrap().data, 57);
        e.map(|v| (*v).data = 75);
    }

    assert_eq!(l, gen_list(&[32, 24, 75]));
}

fn iter_remove() {
    {
        let mut l = gen_list(&[23, 42, 57]);

        {
            let mut it = l.iter_mut();
            assert_eq!(it.remove(), None);

            let e = it.next();
            assert_eq!(e.as_ref().unwrap().data, 23);
            assert_eq!(it.remove().unwrap().data, 23);

            let e = it.next();
            assert_eq!(e.as_ref().unwrap().data, 42);
            assert_eq!(it.remove().unwrap().data, 42);

            let e = it.next();
            assert_eq!(e.as_ref().unwrap().data, 57);
            assert_eq!(it.remove().unwrap().data, 57);

            let e = it.next();
            assert_eq!(e, None);
            assert_eq!(it.remove(), None);
        }

        assert!(l.is_empty());
    }

    {
        let mut l = gen_list(&[1, 2, 3]);

        {
            let mut it = l.iter_mut();
            assert_eq!(it.next().as_ref().unwrap().data, 1);
            assert_eq!(it.next().as_ref().unwrap().data, 2);
            assert_eq!(it.remove().unwrap().data, 2);
            assert_eq!(it.remove().unwrap().data, 1);
            assert_eq!(it.remove(), None);
            assert_eq!(it.next().as_ref().unwrap().data, 3);
        }

        assert_eq!(l, gen_list(&[3]));
    }
}

fn push_back() {
    let mut l = BoxList::new();

    l.push_back(Box::new(TestItem::new(1)));
    l.push_back(Box::new(TestItem::new(2)));
    l.push_back(Box::new(TestItem::new(3)));

    assert_eq!(l, gen_list(&[1, 2, 3]));
}

fn push_front() {
    let mut l = BoxList::new();

    l.push_front(Box::new(TestItem::new(1)));
    l.push_front(Box::new(TestItem::new(2)));
    l.push_front(Box::new(TestItem::new(3)));

    assert_eq!(l, gen_list(&[3, 2, 1]));
}
