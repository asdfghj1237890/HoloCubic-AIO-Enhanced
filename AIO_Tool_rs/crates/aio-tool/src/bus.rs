//! Cross-thread event bus — background workers push progress / completion
//! events; the egui main loop drains them every frame.
//!
//! Design: `std::sync::mpsc` channel (multi-producer / single-consumer).
//! The Sender is `Clone` so every spawned worker (flash, convert, …) can
//! own one. The Receiver lives on the egui main thread and is drained
//! non-blocking at the top of `App::update`.
//!
//! The variant set will grow as Tasks 4-8 (Plan 6) and later plans add
//! features. Anything that crosses thread boundaries goes through here.

use aio_converter::ConvertEvent;
use std::sync::mpsc::{channel, Receiver, Sender};

/// One discrete update from a background worker to the GUI.
#[derive(Debug, Clone)]
pub enum AppEvent {
    /// One progress event from the flasher worker. Re-wrapping
    /// `aio_flasher::FlashEvent` keeps the rich event shape
    /// (PartitionStart / Progress / PartitionDone …) intact across the bus.
    Flash(aio_flasher::FlashEvent),
    /// Flash worker finished. `Ok(())` on success; `Err` carries the rendered
    /// error message ready for display.
    FlashFinished(Result<(), String>),
    /// Event from an Image / Video Converter background op.
    Convert(ConvertEvent),
    /// Converter background op finished. `Ok` carries the encoded byte
    /// payload; `Err` carries the rendered error message ready for display.
    ConvertFinished(Result<Vec<u8>, String>),
    /// Settings worker connected the SerialTransport successfully.
    SettingsConnected,
    /// Raw bytes the Settings worker received from the device.
    SettingsReceived(Vec<u8>),
    /// Settings worker terminated (Ok = normal disconnect, Err = error).
    SettingsFinished(Result<(), String>),
}

/// Sender half of the bus. Workers hold cloned copies.
pub type AppEventTx = Sender<AppEvent>;
/// Receiver half of the bus. Only the `App` holds this.
pub type AppEventRx = Receiver<AppEvent>;

/// Construct a fresh unbounded channel pair.
pub fn channel_pair() -> (AppEventTx, AppEventRx) {
    channel()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn channel_pair_round_trips_events() {
        let (tx, rx) = channel_pair();
        tx.send(AppEvent::Flash(aio_flasher::FlashEvent::Progress {
            index: 1,
            bytes_written: 2048,
        }))
        .expect("send");

        match rx.try_recv().expect("recv") {
            AppEvent::Flash(aio_flasher::FlashEvent::Progress {
                index,
                bytes_written,
            }) => {
                assert_eq!(index, 1);
                assert_eq!(bytes_written, 2048);
            }
            other => panic!("wrong variant: {other:?}"),
        }
    }

    #[test]
    fn try_recv_is_empty_when_no_events() {
        let (_tx, rx) = channel_pair();
        assert!(rx.try_recv().is_err());
    }

    #[test]
    fn sender_is_clonable_for_multi_producer() {
        let (tx, rx) = channel_pair();
        let tx2 = tx.clone();
        tx.send(AppEvent::FlashFinished(Ok(()))).unwrap();
        tx2.send(AppEvent::FlashFinished(Err("b".into()))).unwrap();
        assert!(matches!(
            rx.try_recv().unwrap(),
            AppEvent::FlashFinished(Ok(()))
        ));
        assert!(matches!(
            rx.try_recv().unwrap(),
            AppEvent::FlashFinished(Err(_))
        ));
    }
}
