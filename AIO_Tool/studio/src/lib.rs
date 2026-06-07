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
//! `flash-sim.jsx`'s mock state machine gets replaced incrementally —
//! Phase 1 wires `list_ports` so the prototype shows real COM ports.

mod commands;

/// Entry point — set up Tauri and run the desktop event loop.
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![commands::list_ports])
        .run(tauri::generate_context!())
        .expect("aio-studio: failed to launch the Tauri runtime");
}
