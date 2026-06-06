//! Unit tests for `App::drain_events`. Plan 6 reviewer's recommendation
//! to extract the event-routing logic so it's testable without an egui
//! context.

use aio_flasher::FlashEvent;
use aio_tool::app::App;
use aio_tool::bus::AppEvent;

#[test]
fn flash_progress_appends_log_line() {
    let mut app = App::test_only_new();
    app.bus_tx()
        .send(AppEvent::Flash(FlashEvent::EraseStart))
        .unwrap();
    app.drain_events();
    assert_eq!(app.flasher().log.len(), 1);
}

#[test]
fn flash_finished_ok_logs_complete_when_not_cancelled() {
    let mut app = App::test_only_new();
    app.flasher_mut().busy = true;
    app.bus_tx().send(AppEvent::FlashFinished(Ok(()))).unwrap();
    app.drain_events();
    assert!(!app.flasher().busy);
    assert_eq!(app.flasher().log.len(), 1);
    assert!(app.flasher().log.tail().contains("complete"));
}

#[test]
fn flash_finished_ok_logs_cancelled_when_cancel_flag_set() {
    let mut app = App::test_only_new();
    app.flasher_mut().busy = true;
    app.flasher()
        .cancel
        .store(true, std::sync::atomic::Ordering::Relaxed);
    app.bus_tx().send(AppEvent::FlashFinished(Ok(()))).unwrap();
    app.drain_events();
    assert!(!app.flasher().busy);
    assert!(app.flasher().log.tail().contains("cancelled"));
}
