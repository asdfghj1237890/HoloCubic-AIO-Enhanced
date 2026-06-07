// Prevents a console window from popping up alongside the GUI on Windows
// release builds. `cargo run` (dev) still gets the console for stderr.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    aio_studio_lib::run()
}
