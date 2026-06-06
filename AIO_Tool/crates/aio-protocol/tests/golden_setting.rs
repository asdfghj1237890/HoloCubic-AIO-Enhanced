//! Wire-format goldens for SettingMsg.

use aio_protocol::{ActionType, ModuleType, SettingMsg, ValueType};

fn assert_hex(actual: &[u8], golden: &str) {
    assert_eq!(hex::encode(actual), golden.trim());
}

#[test]
fn setting_get_sys_ssid() {
    let msg = SettingMsg::get("sys", "ssid");
    let bytes = msg.to_wire().unwrap();
    assert_hex(&bytes, include_str!("golden/setting_get_sys_ssid.hex"));
}

#[test]
fn setting_set_zhixin_cityname() {
    let mut msg = SettingMsg::set("zhixin", "cityname", ValueType::String, "Taipei");
    msg.header.from = ModuleType::ToolSettings;
    msg.header.to = ModuleType::CubicSettings;
    msg.header.action = ActionType::SettingSet;
    let bytes = msg.to_wire().unwrap();
    assert_hex(
        &bytes,
        include_str!("golden/setting_set_zhixin_cityname.hex"),
    );
}
