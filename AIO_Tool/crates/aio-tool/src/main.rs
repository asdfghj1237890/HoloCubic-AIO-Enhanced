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
            install_studio_fonts(&cc.egui_ctx);
            cc.egui_ctx.set_visuals(theme::dark_visuals());
            Ok(Box::new(App::new(cc)))
        }),
    )
}

/// Register the Studio design system's fonts (Manrope, Space Grotesk,
/// JetBrains Mono) plus a CJK fallback. Embeds the TTFs directly into the
/// binary via `include_bytes!`, so the running app never needs to read
/// font files from disk.
///
/// Family mapping:
/// - `egui::FontFamily::Proportional` → Manrope (body text)
/// - Custom `"display"` family → Space Grotesk (page titles, step labels)
/// - `egui::FontFamily::Monospace` → JetBrains Mono (log, addresses, port)
/// - All families fall back to a system CJK font for Chinese characters.
fn install_studio_fonts(ctx: &egui::Context) {
    let mut fonts = egui::FontDefinitions::default();

    fonts.font_data.insert(
        "Manrope".to_owned(),
        egui::FontData::from_static(include_bytes!("../../../assets/fonts/Manrope-Regular.ttf")),
    );
    fonts.font_data.insert(
        "Manrope-SemiBold".to_owned(),
        egui::FontData::from_static(include_bytes!("../../../assets/fonts/Manrope-SemiBold.ttf")),
    );
    fonts.font_data.insert(
        "Manrope-Bold".to_owned(),
        egui::FontData::from_static(include_bytes!("../../../assets/fonts/Manrope-Bold.ttf")),
    );
    fonts.font_data.insert(
        "SpaceGrotesk".to_owned(),
        egui::FontData::from_static(include_bytes!(
            "../../../assets/fonts/SpaceGrotesk-Regular.ttf"
        )),
    );
    fonts.font_data.insert(
        "SpaceGrotesk-SemiBold".to_owned(),
        egui::FontData::from_static(include_bytes!(
            "../../../assets/fonts/SpaceGrotesk-Medium.ttf"
        )),
    );
    fonts.font_data.insert(
        "JetBrainsMono".to_owned(),
        egui::FontData::from_static(include_bytes!(
            "../../../assets/fonts/JetBrainsMono-Regular.ttf"
        )),
    );

    // Optional CJK fallback — Latin-only Studio fonts can't render zh
    // labels, so we still probe for an OS Chinese font.
    let cjk_candidates: &[&str] = if cfg!(target_os = "windows") {
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
    let cjk_bytes = cjk_candidates.iter().find_map(|p| std::fs::read(p).ok());
    if let Some(bytes) = cjk_bytes {
        // Baseline-align Microsoft YaHei / PingFang / Noto Sans CJK
        // with Manrope.
        //
        // egui sets the row height from the first font in the family
        // (Manrope), then renders fallback fonts inside that row. The
        // CJK characters have a taller em than Latin, so by default they
        // sit slightly LOWER than Latin glyphs — the Latin baseline
        // floats above the CJK baseline.
        //
        // `y_offset_factor: -0.04` nudges the CJK glyphs up so their
        // baseline meets Latin's. `scale: 1.05` enlarges them a touch
        // since Manrope's em is comparatively short; without this CJK
        // looks visually smaller than the Latin around it. Empirical
        // values — re-tune if a different OS font is picked up.
        let cjk_data = egui::FontData::from_owned(bytes).tweak(egui::FontTweak {
            scale: 1.05,
            y_offset_factor: -0.12,
            y_offset: 0.0,
            baseline_offset_factor: 0.0,
        });
        fonts.font_data.insert("system_cjk".to_owned(), cjk_data);
    } else {
        eprintln!("aio-tool: no CJK system font found; non-Latin labels may render as tofu");
    }

    // Proportional family = Manrope, then SemiBold, then CJK fallback.
    if let Some(prop) = fonts.families.get_mut(&egui::FontFamily::Proportional) {
        prop.clear();
        prop.push("Manrope".to_owned());
        prop.push("Manrope-SemiBold".to_owned());
        prop.push("Manrope-Bold".to_owned());
        if fonts.font_data.contains_key("system_cjk") {
            prop.push("system_cjk".to_owned());
        }
    }
    // Display family — custom — Space Grotesk first, fall back to Manrope.
    fonts.families.insert(
        egui::FontFamily::Name("display".into()),
        vec![
            "SpaceGrotesk-SemiBold".to_owned(),
            "SpaceGrotesk".to_owned(),
            "Manrope-SemiBold".to_owned(),
            "Manrope".to_owned(),
            "system_cjk".to_owned(),
        ]
        .into_iter()
        .filter(|name| fonts.font_data.contains_key(name))
        .collect(),
    );
    // Monospace = JetBrains Mono + CJK fallback.
    if let Some(mono) = fonts.families.get_mut(&egui::FontFamily::Monospace) {
        mono.clear();
        mono.push("JetBrainsMono".to_owned());
        if fonts.font_data.contains_key("system_cjk") {
            mono.push("system_cjk".to_owned());
        }
    }

    ctx.set_fonts(fonts);
}
