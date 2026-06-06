//! Progress events emitted during conversion.

/// One event emitted from `Converter::encode_bin` or `encode_c_array`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConvertEvent {
    /// Encoding started — `total_rows` rows will be processed.
    Start {
        /// Image height in pixels (number of rows the encoder will iterate).
        total_rows: u32,
    },
    /// Progress update — `rows_processed` rows have been encoded.
    Progress {
        /// Number of rows processed so far (0 ≤ value ≤ total_rows).
        rows_processed: u32,
    },
    /// Encoding finished successfully.
    Done,
}
