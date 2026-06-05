//! The egui app — main frame loop + tab dispatch.

use aio_i18n::t;
use eframe::Frame;
use egui::{CentralPanel, Context, TopBottomPanel};

use crate::tabs::{self, Tab};

/// Top-level egui app state.
pub struct App {
    active_tab: Tab,
    // Tab states will be added in later tasks. Flasher state lands in Task 4.
}

impl App {
    /// Construct fresh app state.
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        Self {
            active_tab: Tab::Flasher,
        }
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &Context, _frame: &mut Frame) {
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
            Tab::Flasher => tabs::flasher::show(ui),
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
