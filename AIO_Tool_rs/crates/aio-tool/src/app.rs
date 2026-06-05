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
        // frame. Tabs will consume their own variants in Tasks 4-8;
        // for now we just discard so the channel doesn't grow unbounded.
        while let Ok(_evt) = self.bus_rx.try_recv() {}

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
            Tab::Flasher => tabs::flasher::show(ui, &mut self.flasher),
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
