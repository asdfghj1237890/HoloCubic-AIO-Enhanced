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

    /// Cloneable sender for spawning workers.
    ///
    /// Unused while every tab is still a stub; will be wired in Tasks 4-8.
    #[allow(dead_code)]
    pub(crate) fn bus_tx(&self) -> AppEventTx {
        self.bus_tx.clone()
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &Context, _frame: &mut Frame) {
        // Drain any events the background workers produced since the last
        // frame and surface them in the relevant tab's operation log.
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
                    let line = match result {
                        Ok(()) => "Operation complete.".to_owned(),
                        Err(msg) => format!("Error: {msg}"),
                    };
                    self.flasher.log.push(line);
                }
                crate::bus::AppEvent::Convert { .. } | crate::bus::AppEvent::ConvertFinished(_) => {
                    // Plan 9 handler — drop silently for now.
                }
            }
        }

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
            Tab::Remote => tabs::remote::show(ui),
            Tab::FileManager => tabs::file_manager::show(ui),
            Tab::ImageConverter => tabs::image_converter::show(ui),
            Tab::VideoConverter => tabs::video_converter::show(ui),
            Tab::ToolSettings => tabs::tool_settings::show(ui),
            Tab::Help => tabs::help::show(ui),
        });
    }
}
