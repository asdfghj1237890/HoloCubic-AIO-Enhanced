"""Shared pytest configuration.

Adds the AIO_Tool project root to ``sys.path`` so test modules can
``from util.x import ...`` without installing the project as a package.
"""

from __future__ import annotations

import sys
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))
