//! Help tab — Studio-style layout: PageHeader + intro paragraph + 3
//! sections of clickable link cards. Mirrors `StudioHelp` in the
//! prototype's `studio-convert.jsx`.

use aio_i18n::t;
use egui::{Color32, RichText, ScrollArea, Ui};

use crate::theme;
use crate::widgets::page;
use crate::widgets::studio;

/// One link card row inside a HelpSection.
struct HelpLink {
    title: &'static str,
    sub: &'static str,
    url: &'static str,
    /// Accent color override for the icon square.
    color: Color32,
}

fn section(ui: &mut Ui, label: &str, links: &[HelpLink]) {
    page::section_label(ui, label);
    ui.add_space(theme::S2);
    page::group_card(ui, |ui| {
        for (i, link) in links.iter().enumerate() {
            if i > 0 {
                studio::section_divider(ui);
            }
            ui.horizontal(|ui| {
                // 36×36 icon square with accent-colored center glyph.
                let (rect, _) =
                    ui.allocate_exact_size(egui::Vec2::splat(36.0), egui::Sense::hover());
                let p = ui.painter();
                p.rect_filled(rect, egui::Rounding::same(theme::R2), theme::PANEL_3);
                p.text(
                    rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "↗",
                    egui::FontId::proportional(16.0),
                    link.color,
                );
                ui.add_space(theme::S3);
                ui.vertical(|ui| {
                    ui.label(
                        RichText::new(link.title)
                            .strong()
                            .size(13.5)
                            .color(theme::TEXT),
                    );
                    ui.hyperlink_to(
                        RichText::new(link.sub)
                            .monospace()
                            .size(11.5)
                            .color(theme::TEXT_MUTE),
                        link.url,
                    );
                });
            });
            ui.add_space(theme::S2);
        }
    });
    ui.add_space(theme::S5);
}

/// Render the Help tab.
pub fn show(ui: &mut Ui) {
    page::page_header(ui, &t("tab_help", None), &t("help_subtitle", None), |ui| {
        studio::status_chip(ui, studio::ChipKind::Inactive, "v3.0.0");
    });

    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                ui.set_max_width(720.0);

                // Intro paragraph.
                ui.label(
                    RichText::new(t("help_intro", None))
                        .size(13.5)
                        .color(theme::TEXT_DIM),
                );
                ui.add_space(theme::S5);

                section(
                    ui,
                    &t("help_section_firmware", None),
                    &[
                        HelpLink {
                            title: "HoloCubic AIO Enhanced",
                            sub: "github.com/asdfghj1237890/HoloCubic-AIO-Enhanced",
                            url: "https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced",
                            color: theme::ACCENT,
                        },
                        HelpLink {
                            title: "Demo video",
                            sub: "bilibili.com/video/BV1wS4y1R7YF",
                            url: "https://www.bilibili.com/video/BV1wS4y1R7YF?p=1",
                            color: Color32::from_rgb(0xa7, 0x8b, 0xfa),
                        },
                    ],
                );

                section(
                    ui,
                    &t("help_section_original", None),
                    &[
                        HelpLink {
                            title: "HoloCubic AIO (original)",
                            sub: "github.com/ClimbSnail/HoloCubic_AIO",
                            url: "https://github.com/ClimbSnail/HoloCubic_AIO",
                            color: theme::WARN,
                        },
                        HelpLink {
                            title: "HoloCubic AIO (Gitee mirror)",
                            sub: "gitee.com/ClimbSnailQ/HoloCubic_AIO",
                            url: "https://gitee.com/ClimbSnailQ/HoloCubic_AIO",
                            color: theme::WARN,
                        },
                    ],
                );

                section(
                    ui,
                    &t("help_section_hardware", None),
                    &[HelpLink {
                        title: "HoloCubic hardware",
                        sub: "github.com/peng-zhihui/HoloCubic",
                        url: "https://github.com/peng-zhihui/HoloCubic",
                        color: theme::ACCENT,
                    }],
                );

                ui.label(
                    RichText::new("HoloCubic AIO Tool · v3.0.0")
                        .size(11.5)
                        .color(theme::TEXT_MUTE),
                );
            });
        });
}
