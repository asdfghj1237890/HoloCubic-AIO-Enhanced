//! aio-studio — Tauri 2 host that renders the prototype HTML as its UI.
//!
//! The prototype lives in `Docs/design/studio-flasher/` and is loaded
//! directly as the frontend (`frontendDist` in `tauri.conf.json`). All
//! Studio rendering — typography, icons, transitions, layout — comes
//! from the system WebView (WebView2 / WKWebView / WebKitGTK), giving
//! pixel-identical match to the design.
//!
//! Bridges Rust ↔ JS via Tauri commands. Each `#[tauri::command]` in
//! `commands.rs` becomes an `invoke()`-able function on the JS side.
//! Background flash / erase work emits `flash:event` and
//! `flash:finished` events that the prototype's `useFlasher` hook
//! subscribes to via `window.__TAURI__.event.listen`.

mod commands;
mod fm;
mod img;
mod video;

/// Entry point — set up Tauri and run the desktop event loop.
pub fn run() {
    tauri::Builder::default()
        // Single-instance MUST be the first plugin per Tauri docs —
        // when a second launch fires, the callback runs on the FIRST
        // instance and we re-focus its main window.
        .plugin(tauri_plugin_single_instance::init(|app, _argv, _cwd| {
            use tauri::Manager;
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.unminimize();
                let _ = window.show();
                let _ = window.set_focus();
            }
        }))
        .plugin(tauri_plugin_shell::init())
        .manage(commands::ConnState::default())
        .invoke_handler(tauri::generate_handler![
            commands::list_ports,
            commands::connect_device,
            commands::disconnect_device,
            commands::start_flash,
            commands::start_erase,
            commands::cancel_op,
            commands::send_remote,
            commands::reboot_device,
            commands::list_setting_keys,
            commands::read_all_settings,
            commands::write_changed_settings,
            commands::fm_connect,
            commands::fm_disconnect,
            commands::fm_list_dir,
            commands::fm_read_file,
            commands::fm_remove,
            commands::fm_rename,
            commands::fm_get_info,
            commands::convert_pick_images,
            commands::convert_image_batch,
            commands::convert_image_cancel,
            commands::video_ffmpeg_check,
            commands::video_pick_source,
            commands::video_pick_output,
            commands::video_run,
            commands::video_cancel,
        ])
        .run(tauri::generate_context!())
        .expect("aio-studio: failed to launch the Tauri runtime");
}
