//! Supported user-interface languages.

use std::fmt;
use std::str::FromStr;

/// One of the three locales supported by the HoloCubic AIO tool.
///
/// Order: ZhCn, ZhTw, EnUs — matches `I18n.get_available_languages()` from
/// `util/i18n.py` so the Tool Settings tab renders radio buttons in the same
/// order users are accustomed to (see Plan 2 D6).
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum Lang {
    /// Simplified Chinese (`zh_CN`) — default.
    ZhCn,
    /// Traditional Chinese (`zh_TW`).
    ZhTw,
    /// English (`en_US`).
    EnUs,
}

impl Lang {
    /// The default language used when no `config.json` is present (D3).
    pub const DEFAULT: Self = Self::ZhCn;

    /// All locales in display order (D6).
    pub const ALL: [Self; 3] = [Self::ZhCn, Self::ZhTw, Self::EnUs];

    /// The wire / config-file code (e.g. `"zh_CN"`).
    pub fn code(self) -> &'static str {
        match self {
            Self::ZhCn => "zh_CN",
            Self::ZhTw => "zh_TW",
            Self::EnUs => "en_US",
        }
    }

    /// The native display name (e.g. `"简体中文"`).
    pub fn display_name(self) -> &'static str {
        match self {
            Self::ZhCn => "简体中文",
            Self::ZhTw => "繁體中文",
            Self::EnUs => "English",
        }
    }
}

impl fmt::Display for Lang {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.code())
    }
}

impl FromStr for Lang {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "zh_CN" => Ok(Self::ZhCn),
            "zh_TW" => Ok(Self::ZhTw),
            "en_US" => Ok(Self::EnUs),
            _ => Err(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lang_codes_match_python_constants() {
        assert_eq!(Lang::ZhCn.code(), "zh_CN");
        assert_eq!(Lang::ZhTw.code(), "zh_TW");
        assert_eq!(Lang::EnUs.code(), "en_US");
    }

    #[test]
    fn lang_display_names_match_python_constants() {
        assert_eq!(Lang::ZhCn.display_name(), "简体中文");
        assert_eq!(Lang::ZhTw.display_name(), "繁體中文");
        assert_eq!(Lang::EnUs.display_name(), "English");
    }

    #[test]
    fn lang_all_order_matches_python() {
        assert_eq!(Lang::ALL, [Lang::ZhCn, Lang::ZhTw, Lang::EnUs]);
    }

    #[test]
    fn lang_default_is_zh_cn() {
        assert_eq!(Lang::DEFAULT, Lang::ZhCn);
    }

    #[test]
    fn parse_known_codes() {
        for &l in &Lang::ALL {
            assert_eq!(l.code().parse::<Lang>().unwrap(), l);
        }
    }

    #[test]
    fn parse_unknown_returns_err() {
        assert_eq!("ja_JP".parse::<Lang>(), Err(()));
        assert_eq!("".parse::<Lang>(), Err(()));
        assert_eq!("zh".parse::<Lang>(), Err(()));
    }

    #[test]
    fn display_roundtrips_via_parse() {
        for &l in &Lang::ALL {
            let s = l.to_string();
            let parsed: Lang = s.parse().unwrap();
            assert_eq!(parsed, l);
        }
    }
}
