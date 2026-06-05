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

use std::sync::mpsc::{channel, Receiver, Sender};

/// One discrete update from a background worker to the GUI.
#[derive(Debug, Clone)]
pub enum AppEvent {
    /// Flash progress update (0.0..=1.0).
    Flash {
        /// Fraction complete.
        fraction: f32,
        /// Human-readable status line (e.g. "Writing bootloader…").
        message: String,
    },
    /// Image / video conversion progress update (0.0..=1.0).
    Convert {
        /// Fraction complete.
        fraction: f32,
        /// Human-readable status line.
        message: String,
    },
    /// Flash worker finished. `Ok` carries a brief summary; `Err` carries the
    /// rendered error message ready for display.
    FlashFinished(Result<String, String>),
    /// Convert worker finished. Same shape as `FlashFinished`.
    ConvertFinished(Result<String, String>),
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
        tx.send(AppEvent::Flash {
            fraction: 0.5,
            message: "halfway".into(),
        })
        .expect("send");

        match rx.try_recv().expect("recv") {
            AppEvent::Flash { fraction, message } => {
                assert!((fraction - 0.5).abs() < f32::EPSILON);
                assert_eq!(message, "halfway");
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
        tx.send(AppEvent::FlashFinished(Ok("a".into()))).unwrap();
        tx2.send(AppEvent::FlashFinished(Err("b".into()))).unwrap();
        assert!(matches!(
            rx.try_recv().unwrap(),
            AppEvent::FlashFinished(Ok(_))
        ));
        assert!(matches!(
            rx.try_recv().unwrap(),
            AppEvent::FlashFinished(Err(_))
        ));
    }
}
