"""Tests for util.i18n — JSON loading, language switching, fallbacks."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from util import i18n as i18n_mod
from util.i18n import I18n, get_i18n, t


@pytest.fixture(autouse=True)
def _reset_singleton(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Reset the I18n singleton + isolate from any on-disk tool_config.json.

    Without the monkeypatch, a tool_config.json left over from manual usage
    (e.g. user switched language to zh_TW in the GUI) would override the
    expected default language and break the default-language assertions.
    """
    # Point _config_file at a guaranteed-nonexistent path inside tmp_path
    monkeypatch.setattr(I18n, "_config_file", str(tmp_path / "no_such_config.json"))
    I18n._instance = None
    I18n._current_language = I18n.LANG_ZH_CN
    I18n.TRANSLATIONS = {}
    i18n_mod._i18n_instance = None
    yield
    # Re-initialize singleton state cleanly after the test runs so that
    # subsequent tests in this session can rely on real files being loaded.
    I18n._instance = None
    i18n_mod._i18n_instance = None


class TestJsonLoading:
    def test_all_three_languages_load(self) -> None:
        i = get_i18n()
        codes = [code for code, _name in i.get_available_languages()]
        assert codes == ["zh_CN", "zh_TW", "en_US"]

    def test_translations_have_keys(self) -> None:
        get_i18n()  # forces load
        # Each language should have a non-trivial number of keys
        for lang_code in ("zh_CN", "zh_TW", "en_US"):
            assert len(I18n.TRANSLATIONS[lang_code]) > 50, (
                f"{lang_code} should have >50 translation keys"
            )

    def test_language_files_have_same_keys(self) -> None:
        """All three languages should expose the same key set (no missing translations)."""
        get_i18n()
        zh_cn_keys = set(I18n.TRANSLATIONS["zh_CN"].keys())
        zh_tw_keys = set(I18n.TRANSLATIONS["zh_TW"].keys())
        en_us_keys = set(I18n.TRANSLATIONS["en_US"].keys())
        assert zh_cn_keys == zh_tw_keys, (
            f"zh_CN and zh_TW differ: only_in_cn={zh_cn_keys - zh_tw_keys}, "
            f"only_in_tw={zh_tw_keys - zh_cn_keys}"
        )
        assert zh_cn_keys == en_us_keys, (
            f"zh_CN and en_US differ: only_in_cn={zh_cn_keys - en_us_keys}, "
            f"only_in_en={en_us_keys - zh_cn_keys}"
        )


class TestLanguageSwitching:
    def test_default_language_is_zh_cn(self) -> None:
        i = get_i18n()
        assert i.get_language() == "zh_CN"

    def test_set_language_persists_across_calls(self) -> None:
        i = get_i18n()
        assert i.set_language("en_US") is True
        assert i.get_language() == "en_US"
        assert get_i18n().get_language() == "en_US"

    def test_set_unsupported_language_returns_false(self) -> None:
        i = get_i18n()
        assert i.set_language("ja_JP") is False
        # Language unchanged
        assert i.get_language() == "zh_CN"


class TestTranslation:
    def test_translation_differs_per_language(self) -> None:
        i = get_i18n()
        i.set_language("zh_CN")
        zh_cn = t("tab_help")
        i.set_language("zh_TW")
        zh_tw = t("tab_help")
        i.set_language("en_US")
        en_us = t("tab_help")
        assert zh_cn != zh_tw or zh_cn != en_us, (
            "tab_help should differ across at least two languages"
        )
        assert en_us == "Help"

    def test_missing_key_falls_back_to_key_name(self) -> None:
        result = t("this_key_definitely_does_not_exist")
        assert result == "this_key_definitely_does_not_exist"

    def test_missing_key_with_default_returns_default(self) -> None:
        result = t("this_key_definitely_does_not_exist", default="fallback")
        assert result == "fallback"

    def test_get_language_name(self) -> None:
        i = get_i18n()
        assert i.get_language_name("zh_CN") == "简体中文"
        assert i.get_language_name("zh_TW") == "繁體中文"
        assert i.get_language_name("en_US") == "English"


class TestRobustness:
    def test_invalid_json_returns_empty_dict(
        self,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """A malformed JSON file should not crash the loader."""
        bad = tmp_path / "broken.json"
        bad.write_text("{not valid json", encoding="utf-8")
        result = (
            i18n_mod._load_translation.__wrapped__
            if hasattr(
                i18n_mod._load_translation,
                "__wrapped__",
            )
            else i18n_mod._load_translation
        )
        # Patch the dir to point at our tmp dir
        monkeypatch.setattr(i18n_mod, "_i18n_dir", lambda: tmp_path)
        assert result("broken") == {}

    def test_missing_file_returns_empty_dict(
        self,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        monkeypatch.setattr(i18n_mod, "_i18n_dir", lambda: tmp_path)
        assert i18n_mod._load_translation("nonexistent") == {}

    def test_non_object_json_returns_empty_dict(
        self,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        # JSON arrays / scalars are not valid translation tables
        bad = tmp_path / "list.json"
        bad.write_text(json.dumps(["not", "a", "dict"]), encoding="utf-8")
        monkeypatch.setattr(i18n_mod, "_i18n_dir", lambda: tmp_path)
        assert i18n_mod._load_translation("list") == {}
