"""Centralized logging configuration for AIO_Tool."""
from __future__ import annotations

import logging
import sys
from pathlib import Path


def setup_logging(level: int = logging.INFO) -> None:
    """Configure root logger with console + file handlers."""
    log_dir = Path("OutFile")
    log_dir.mkdir(exist_ok=True)

    fmt = "%(asctime)s [%(levelname)s] %(name)s: %(message)s"
    logging.basicConfig(
        level=level,
        format=fmt,
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler(log_dir / "aio_tool.log", encoding="utf-8"),
        ],
    )


def get_logger(name: str) -> logging.Logger:
    """Get a named logger. Usage: logger = get_logger(__name__)"""
    return logging.getLogger(name)
