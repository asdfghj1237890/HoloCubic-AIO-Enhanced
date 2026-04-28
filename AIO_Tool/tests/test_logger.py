"""Tests for util.logger — setup_logging + get_logger."""

from __future__ import annotations

import logging
from pathlib import Path

import pytest

from util.logger import get_logger, setup_logging


def test_get_logger_returns_named_logger() -> None:
    log = get_logger("test_module")
    assert isinstance(log, logging.Logger)
    assert log.name == "test_module"


def test_get_logger_is_idempotent() -> None:
    a = get_logger("same_name")
    b = get_logger("same_name")
    assert a is b


def test_setup_logging_creates_outfile_dir(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.chdir(tmp_path)
    setup_logging(level=logging.DEBUG)

    # OutFile/aio_tool.log should now exist
    log_file = tmp_path / "OutFile" / "aio_tool.log"
    assert log_file.parent.exists()


def test_setup_logging_writes_to_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.chdir(tmp_path)
    # Reset root handlers so setup_logging fully reconfigures
    for h in list(logging.root.handlers):
        logging.root.removeHandler(h)

    setup_logging(level=logging.INFO)
    log = get_logger("smoke_test")
    log.info("hello from test")

    # Flush handlers
    for h in logging.root.handlers:
        h.flush()

    log_file = tmp_path / "OutFile" / "aio_tool.log"
    assert log_file.exists()
    content = log_file.read_text(encoding="utf-8")
    assert "hello from test" in content
    assert "smoke_test" in content
