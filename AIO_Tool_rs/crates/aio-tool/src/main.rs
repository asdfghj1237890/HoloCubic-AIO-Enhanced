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
            cc.egui_ctx.set_visuals(theme::dark_visuals());
            Ok(Box::new(App::new(cc)))
        }),
    )
}
