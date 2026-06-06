//! Cross-language golden tests for translation byte-equality.
//!
//! Each test sets the global language, calls `t()`, and compares against
//! the byte-exact Python output stored under `golden/`.

use std::sync::Mutex;

use aio_i18n::{get_i18n, t, Lang};

// Integration tests run in one binary but `cargo test` may parallelize.
// Serialize so language switches don't race.
static LANG_LOCK: Mutex<()> = Mutex::new(());

fn check(key: &str, lang: Lang, golden: &str) {
    let _g = LANG_LOCK.lock().expect("lang lock poisoned");
    get_i18n().set_language(lang);
    let actual = t(key, None);
    assert_eq!(actual, golden, "key={key} lang={}", lang.code());
}

#[test]
fn tab_help_en_us() {
    check(
        "tab_help",
        Lang::EnUs,
        include_str!("golden/tab_help.en_US.txt"),
    );
}
#[test]
fn tab_help_zh_cn() {
    check(
        "tab_help",
        Lang::ZhCn,
        include_str!("golden/tab_help.zh_CN.txt"),
    );
}
#[test]
fn tab_help_zh_tw() {
    check(
        "tab_help",
        Lang::ZhTw,
        include_str!("golden/tab_help.zh_TW.txt"),
    );
}

#[test]
fn app_title_en_us() {
    check(
        "app_title",
        Lang::EnUs,
        include_str!("golden/app_title.en_US.txt"),
    );
}
#[test]
fn app_title_zh_tw() {
    check(
        "app_title",
        Lang::ZhTw,
        include_str!("golden/app_title.zh_TW.txt"),
    );
}

#[test]
fn ok_en_us() {
    check("ok", Lang::EnUs, include_str!("golden/ok.en_US.txt"));
}

#[test]
fn port_number_en_us() {
    check(
        "port_number",
        Lang::EnUs,
        include_str!("golden/port_number.en_US.txt"),
    );
}

#[test]
fn flash_firmware_en_us() {
    check(
        "flash_firmware",
        Lang::EnUs,
        include_str!("golden/flash_firmware.en_US.txt"),
    );
}
#[test]
fn flash_firmware_zh_cn() {
    check(
        "flash_firmware",
        Lang::ZhCn,
        include_str!("golden/flash_firmware.zh_CN.txt"),
    );
}

#[test]
fn language_changed_en_us() {
    check(
        "language_changed",
        Lang::EnUs,
        include_str!("golden/language_changed.en_US.txt"),
    );
}
#[test]
fn language_changed_zh_cn() {
    check(
        "language_changed",
        Lang::ZhCn,
        include_str!("golden/language_changed.zh_CN.txt"),
    );
}

#[test]
fn language_label_en_us() {
    check(
        "language_label",
        Lang::EnUs,
        include_str!("golden/language_label.en_US.txt"),
    );
}
#[test]
fn language_label_zh_tw() {
    check(
        "language_label",
        Lang::ZhTw,
        include_str!("golden/language_label.zh_TW.txt"),
    );
}

#[test]
fn help_info_zh_cn() {
    check(
        "help_info",
        Lang::ZhCn,
        include_str!("golden/help_info.zh_CN.txt"),
    );
}

#[test]
fn image_converter_info_zh_tw() {
    check(
        "image_converter_info",
        Lang::ZhTw,
        include_str!("golden/image_converter_info.zh_TW.txt"),
    );
}
