//! Scrollable append-only operation log used by Flasher (and later File
//! Manager / Settings).

use egui::{ScrollArea, Ui};

/// Append-only log buffer with a scrollable read-only view.
#[derive(Default)]
pub struct OperationLog {
    lines: Vec<String>,
}

impl OperationLog {
    /// Append a message. Line is shown at the bottom; older lines scroll up.
    pub fn push(&mut self, msg: impl Into<String>) {
        self.lines.push(msg.into());
    }

    /// Clear all lines.
    pub fn clear(&mut self) {
        self.lines.clear();
    }

    /// Number of buffered lines (useful for tests).
    pub fn len(&self) -> usize {
        self.lines.len()
    }

    /// True when no lines have been pushed.
    pub fn is_empty(&self) -> bool {
        self.lines.is_empty()
    }

    /// Last pushed line (test convenience). Returns `""` if empty.
    pub fn tail(&self) -> &str {
        self.lines.last().map(String::as_str).unwrap_or("")
    }

    /// Render in a scrollable area; sticks to bottom so latest line is visible.
    pub fn show(&self, ui: &mut Ui) {
        ScrollArea::vertical()
            .stick_to_bottom(true)
            .auto_shrink([false; 2])
            .show(ui, |ui| {
                for line in &self.lines {
                    ui.monospace(line);
                }
            });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_appends() {
        let mut log = OperationLog::default();
        assert!(log.is_empty());
        log.push("first");
        log.push("second");
        assert_eq!(log.len(), 2);
    }

    #[test]
    fn clear_resets() {
        let mut log = OperationLog::default();
        log.push("a");
        log.push("b");
        log.clear();
        assert!(log.is_empty());
    }
}
