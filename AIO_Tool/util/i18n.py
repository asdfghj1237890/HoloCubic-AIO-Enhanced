################################################################################
#
# Internationalization (i18n) module for HoloCubic AIO Tool
# Supports: Simplified Chinese, Traditional Chinese, and English
#
# Translation tables live as JSON files under ``i18n/<lang_code>.json`` so
# they can be edited / contributed without touching Python source.
#
################################################################################

from __future__ import annotations

import json
from pathlib import Path

from util.common import get_resource_path
from util.logger import get_logger

logger = get_logger(__name__)


def _i18n_dir() -> Path:
    """Return the directory containing the per-language JSON files."""
    return Path(get_resource_path("i18n"))


def _load_translation(lang_code: str) -> dict[str, str]:
    """Load a single language file. Returns empty dict on any failure."""
    path = _i18n_dir() / f"{lang_code}.json"
    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict):
            logger.error("i18n file %s did not contain a JSON object", path)
            return {}
        return data
    except FileNotFoundError:
        logger.warning("i18n file missing: %s", path)
        return {}
    except json.JSONDecodeError as err:
        logger.error("i18n file %s is invalid JSON: %s", path, err)
        return {}


class I18n:
    """Internationalization handler (singleton)."""

    # Language codes (kept as class constants for backward compatibility)
    LANG_ZH_CN = "zh_CN"  # Simplified Chinese
    LANG_ZH_TW = "zh_TW"  # Traditional Chinese
    LANG_EN_US = "en_US"  # English

    #: Display names for each supported language
    _LANGUAGE_NAMES: dict[str, str] = {
        LANG_ZH_CN: "简体中文",
        LANG_ZH_TW: "繁體中文",
        LANG_EN_US: "English",
    }

    #: Translations are loaded lazily into this dict by ``__new__``.
    TRANSLATIONS: dict[str, dict[str, str]] = {}

    _instance: I18n | None = None
    _current_language: str = LANG_ZH_CN
    _config_file: str = "tool_config.json"

    def __new__(cls) -> I18n:
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._load_all_translations()
            cls._instance._load_language_preference()
        return cls._instance

    def _load_all_translations(self) -> None:
        """Load every supported language file into ``TRANSLATIONS``."""
        for lang_code in self._LANGUAGE_NAMES:
            type(self).TRANSLATIONS[lang_code] = _load_translation(lang_code)

    def _load_language_preference(self) -> None:
        """Load the user's saved language code from ``tool_config.json``."""
        path = Path(self._config_file)
        try:
            if path.exists():
                with path.open("r", encoding="utf-8") as f:
                    config = json.load(f)
                saved_lang = config.get("language", self.LANG_ZH_CN)
                if saved_lang in self.TRANSLATIONS:
                    type(self)._current_language = saved_lang
        except Exception as err:
            logger.error("Failed to load language preference: %s", err)
            type(self)._current_language = self.LANG_ZH_CN

    def save_language_preference(self, language: str) -> bool:
        """Persist the user's chosen language to ``tool_config.json``."""
        path = Path(self._config_file)
        try:
            config: dict[str, object] = {}
            if path.exists():
                with path.open("r", encoding="utf-8") as f:
                    config = json.load(f)
            config["language"] = language
            with path.open("w", encoding="utf-8") as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
            return True
        except Exception as err:
            logger.error("Failed to save language preference: %s", err)
            return False

    def set_language(self, language: str) -> bool:
        """Set the active language. Returns False if unsupported."""
        if language in self.TRANSLATIONS:
            type(self)._current_language = language
            return True
        return False

    def get_language(self) -> str:
        """Get the active language code."""
        return self._current_language

    def get_language_name(self, lang_code: str) -> str:
        """Return the display name for a language code."""
        return self._LANGUAGE_NAMES.get(lang_code, lang_code)

    def get_available_languages(self) -> list[tuple[str, str]]:
        """List of (code, display_name) pairs for every supported language."""
        return [(code, name) for code, name in self._LANGUAGE_NAMES.items()]

    def t(self, key: str, default: str | None = None) -> str:
        """Translate a key. Falls back to ``default`` or the key itself."""
        lang_dict = self.TRANSLATIONS.get(self._current_language, {})
        return lang_dict.get(key, default if default is not None else key)


# Module-level singleton accessor
_i18n_instance: I18n | None = None


def get_i18n() -> I18n:
    """Get (lazy-init) global i18n instance."""
    global _i18n_instance
    if _i18n_instance is None:
        _i18n_instance = I18n()
    return _i18n_instance


def t(key: str, default: str | None = None) -> str:
    """Shortcut function for translation."""
    return get_i18n().t(key, default)
