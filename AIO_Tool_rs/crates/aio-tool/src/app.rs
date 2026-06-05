//! The egui app — main frame loop + tab dispatch.

use std::time::Duration;

use aio_i18n::t;
use eframe::Frame;
use egui::{CentralPanel, Context, TopBottomPanel};

use crate::bus::{channel_pair, AppEventRx, AppEventTx};
use crate::tabs::{self, Tab};

/// How often to force a frame repaint so background events don't wait on
/// user input to be visible. 100ms ≈ 10 fps "idle" rate is plenty for
/// progress bars; egui still repaints on every interaction in addition.
const IDLE_REPAINT: Duration = Duration::from_millis(100);

/// Top-level egui app state.
pub struct App {
    active_tab: Tab,

    /// Sender end of the cross-thread event bus. Cloned into every
    /// background worker spawned by the tabs. Tabs consume their variants
    /// from `bus_rx` (drained at the top of every frame).
    bus_tx: AppEventTx,
    bus_rx: AppEventRx,

    /// Flasher tab state (Plan 6 Task 4).
    flasher: crate::tabs::flasher::FlasherState,
}

impl App {
    /// Construct fresh app state.
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        let (bus_tx, bus_rx) = channel_pair();
        Self {
            active_tab: Tab::Flasher,
            bus_tx,
            bus_rx,
            flasher: crate::tabs::flasher::FlasherState::default(),
        }
    }

    /// Drain pending background-worker events from the bus and surface them
    /// in the relevant tab's state. Called once per `update` tick; extracted
    /// so the event-routing logic can be unit-tested without an egui
    /// `CreationContext`.
    pub fn drain_events(&mut self) {
        while let Ok(evt) = self.bus_rx.try_recv() {
            match evt {
                crate::bus::AppEvent::Flash(fe) => {
                    let line = match fe {
                        aio_flasher::FlashEvent::EraseStart => "Erasing chip...".to_owned(),
                        aio_flasher::FlashEvent::EraseDone => "Chip erase done.".to_owned(),
                        aio_flasher::FlashEvent::PartitionStart { index, total_bytes } => {
                            format!("Writing partition {index} ({total_bytes} bytes)...")
                        }
                        aio_flasher::FlashEvent::Progress {
                            index,
                            bytes_written,
                        } => format!("  partition {index}: {bytes_written} bytes"),
                        aio_flasher::FlashEvent::PartitionDone { index } => {
                            format!("Partition {index} done.")
                        }
                    };
                    self.flasher.log.push(line);
                }
                crate::bus::AppEvent::FlashFinished(result) => {
                    self.flasher.busy = false;
                    let cancelled = self
                        .flasher
                        .cancel
                        .load(std::sync::atomic::Ordering::Relaxed);
                    let line = match (result, cancelled) {
                        (Ok(()), true) => {
                            "Operation cancelled (chip erase already completed).".to_owned()
                        }
                        (Ok(()), false) => "✓ Operation complete.".to_owned(),
                        (Err(msg), _) => format!("✗ {msg}"),
                    };
                    self.flasher.log.push(line);
                }
                crate::bus::AppEvent::Convert(_) | crate::bus::AppEvent::ConvertFinished(_) => {
                    // Plan 9 handler.
                }
                crate::bus::AppEvent::SettingsConnected => {
                    // Placeholder log; the settings tab's UI handler is in Group C.
                    // For now just log to the FLASHER log so we can see it during dev.
                    // Group C wires this to settings.log instead.
                    self.flasher.log.push("Settings worker connected.");
                }
                crate::bus::AppEvent::SettingsReceived(bytes) => {
                    // Group C decodes via aio_protocol::SettingMsg::from_wire and routes
                    // to per-key state. For Plan 7 Group B (no Settings UI yet), just
                    // log the byte count.
                    self.flasher
                        .log
                        .push(format!("Settings <- {} bytes", bytes.len()));
                }
                crate::bus::AppEvent::SettingsFinished(result) => {
                    let line = match result {
                        Ok(()) => "Settings disconnected.".to_owned(),
                        Err(msg) => format!("Settings worker error: {msg}"),
                    };
                    self.flasher.log.push(line);
                }
            }
        }
    }

    /// Construct an `App` with default state for unit / integration tests.
    /// Skips the egui `CreationContext` requirement of `new`.
    ///
    /// `#[doc(hidden)]` because this is a test seam — not part of the
    /// supported public API — but it cannot be `cfg(test)` since integration
    /// tests under `tests/` compile against the crate as an external
    /// dependency (where `cfg(test)` is not enabled).
    #[doc(hidden)]
    pub fn test_only_new() -> Self {
        let (bus_tx, bus_rx) = crate::bus::channel_pair();
        Self {
            active_tab: crate::tabs::Tab::Flasher,
            flasher: crate::tabs::flasher::FlasherState::default(),
            bus_tx,
            bus_rx,
        }
    }

    /// Borrowed sender for tests that need to inject bus events.
    #[doc(hidden)]
    pub fn bus_tx(&self) -> &crate::bus::AppEventTx {
        &self.bus_tx
    }

    /// Borrowed Flasher state for test assertions.
    #[doc(hidden)]
    pub fn flasher(&self) -> &crate::tabs::flasher::FlasherState {
        &self.flasher
    }

    /// Mutable Flasher state so tests can preset busy / cancel flags.
    #[doc(hidden)]
    pub fn flasher_mut(&mut self) -> &mut crate::tabs::flasher::FlasherState {
        &mut self.flasher
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &Context, _frame: &mut Frame) {
        // Drain any events the background workers produced since the last
        // frame and surface them in the relevant tab's operation log.
        self.drain_events();

        // Ensure background events don't wait for user input to surface.
        ctx.request_repaint_after(IDLE_REPAINT);

        TopBottomPanel::top("tab_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                for &tab in &Tab::ALL {
                    let label = t(tab.i18n_key(), None);
                    if ui.selectable_label(self.active_tab == tab, label).clicked() {
                        self.active_tab = tab;
                    }
                }
            });
        });

        CentralPanel::default().show(ctx, |ui| match self.active_tab {
            Tab::Flasher => tabs::flasher::show(ui, &mut self.flasher, &self.bus_tx),
            Tab::Settings => tabs::settings::show(ui),
            Tab::FileManager => tabs::file_manager::show(ui),
            Tab::ImageConverter => tabs::image_converter::show(ui),
            Tab::VideoConverter => tabs::video_converter::show(ui),
            Tab::ToolSettings => tabs::tool_settings::show(ui),
            Tab::Help => tabs::help::show(ui),
        });
    }
}
