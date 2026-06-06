//! HoloCubic AIO Tool — egui binary entry.

use aio_tool::app::App;
use aio_tool::theme;

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([theme::INITIAL_WIDTH, theme::INITIAL_HEIGHT])
            .with_min_inner_size([theme::MIN_WIDTH, theme::MIN_HEIGHT])
            .with_title("HoloCubic_AIO Tool"),
        ..Default::default()
    };

    eframe::run_native(
        "HoloCubic_AIO Tool",
        options,
        Box::new(|cc| {
            install_cjk_font(&cc.egui_ctx);
            cc.egui_ctx.set_visuals(theme::dark_visuals());
            Ok(Box::new(App::new(cc)))
        }),
    )
}

/// Append a system CJK font as a fallback to the default families. Without
/// this, egui's bundled Latin-only font renders the zh_CN / zh_TW labels as
/// tofu (□). We probe well-known OS paths and silently no-op if none exist.
fn install_cjk_font(ctx: &egui::Context) {
    let candidates: &[&str] = if cfg!(target_os = "windows") {
        &[
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/msjh.ttc",
            "C:/Windows/Fonts/simsun.ttc",
        ]
    } else if cfg!(target_os = "macos") {
        &[
            "/System/Library/Fonts/PingFang.ttc",
            "/System/Library/Fonts/STHeiti Light.ttc",
        ]
    } else {
        &[
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
            "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        ]
    };

    let Some(bytes) = candidates.iter().find_map(|p| std::fs::read(p).ok()) else {
        eprintln!("aio-tool: no CJK system font found; non-Latin labels may render as tofu");
        return;
    };

    let mut fonts = egui::FontDefinitions::default();
    fonts
        .font_data
        .insert("system_cjk".to_owned(), egui::FontData::from_owned(bytes));
    if let Some(prop) = fonts.families.get_mut(&egui::FontFamily::Proportional) {
        prop.push("system_cjk".to_owned());
    }
    if let Some(mono) = fonts.families.get_mut(&egui::FontFamily::Monospace) {
        mono.push("system_cjk".to_owned());
    }
    ctx.set_fonts(fonts);
}
