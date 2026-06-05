//! Wire-format goldens for dir / file messages.

use aio_protocol::file::{
    DirCreate, DirList, DirRemove, DirRename, FileCreate, FileGetInfo, FileRead, FileRemove,
    FileRename, FileWrite,
};

fn assert_hex(actual: &[u8], golden: &str) {
    assert_eq!(hex::encode(actual), golden.trim());
}

#[test]
fn dir_create_root() {
    assert_hex(
        &DirCreate::new("/test").to_wire().unwrap(),
        include_str!("golden/dir_create_root.hex"),
    );
}

#[test]
fn dir_remove_old() {
    assert_hex(
        &DirRemove::new("/old").to_wire().unwrap(),
        include_str!("golden/dir_remove_old.hex"),
    );
}

#[test]
fn dir_rename_a_to_b() {
    assert_hex(
        &DirRename::new("/a", "/b").to_wire().unwrap(),
        include_str!("golden/dir_rename_a_to_b.hex"),
    );
}

#[test]
fn dir_list_sd() {
    assert_hex(
        &DirList::request("/sd").to_wire().unwrap(),
        include_str!("golden/dir_list_sd.hex"),
    );
}

#[test]
fn file_create_foo() {
    assert_hex(
        &FileCreate::new("/sd/foo.bin", 0x1234).to_wire().unwrap(),
        include_str!("golden/file_create_foo.hex"),
    );
}

#[test]
fn file_write_hello() {
    assert_hex(
        &FileWrite::new(b"hello".to_vec()).to_wire().unwrap(),
        include_str!("golden/file_write_hello.hex"),
    );
}

#[test]
fn file_read_log() {
    assert_hex(
        &FileRead::request("/sd/log.txt").to_wire().unwrap(),
        include_str!("golden/file_read_log.hex"),
    );
}

#[test]
fn file_remove_bad() {
    assert_hex(
        &FileRemove::new("/sd/bad.bin").to_wire().unwrap(),
        include_str!("golden/file_remove_bad.hex"),
    );
}

#[test]
fn file_rename_foo() {
    assert_hex(
        &FileRename::new("/sd/foo.txt").to_wire().unwrap(),
        include_str!("golden/file_rename_foo.hex"),
    );
}

#[test]
fn file_get_info_foo() {
    assert_hex(
        &FileGetInfo::request("/sd/foo.txt").to_wire().unwrap(),
        include_str!("golden/file_get_info_foo.hex"),
    );
}
