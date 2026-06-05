//! HoloCubic AIO tool i18n.
//!
//! Provides translation lookup for 140 keys × 3 locales (zh_CN, zh_TW, en_US) and
//! persists the user's language preference. Behavior matches `AIO_Tool/util/i18n.py`
//! per the Discovery section of Plan 2.
#![deny(missing_docs)]
#![deny(unsafe_code)]

pub mod config;
pub mod error;
pub mod i18n;
pub mod lang;

pub use error::{LoadError, SaveError};
pub use i18n::{get_i18n, t, I18n};
pub use lang::Lang;
