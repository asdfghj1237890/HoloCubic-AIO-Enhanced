# -*- coding: utf-8 -*-
################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

from __future__ import annotations

import customtkinter as ctk

#: tk.Entry kwargs that ctk.CTkEntry does NOT accept — silently filtered out
#: so existing call sites passing these don't have to be rewritten yet.
_TK_ONLY_ENTRY_KWARGS: frozenset[str] = frozenset({
    "highlightcolor", "highlightthickness", "highlightbackground",
    "relief", "bd", "borderwidth", "bg", "fg", "selectbackground",
    "selectforeground", "insertbackground", "disabledbackground",
    "disabledforeground", "readonlybackground",
})


def _sanitize_ctk_entry_kwargs(kwargs: dict[str, object]) -> dict[str, object]:
    """Strip tk-only kwargs so call sites written for tk.Entry still work."""
    return {k: v for k, v in kwargs.items() if k not in _TK_ONLY_ENTRY_KWARGS}


class EntryWithPlaceholder(ctk.CTkEntry):
    """CTkEntry-backed entry that shows placeholder text natively.

    Maintains backward compatibility with the old tk.Entry-based class:
    ``placeholder`` and ``placeholder_color`` kwargs map to CTkEntry's
    built-in ``placeholder_text`` / ``placeholder_text_color`` arguments.
    Tk-only kwargs (e.g. ``highlightcolor``) are silently ignored so
    existing call sites don't need to change.
    """

    def __init__(
        self,
        master: ctk.CTkBaseClass | None = None,
        *,
        placeholder: str = "",
        placeholder_color: str = "grey",
        **attribute: object,
    ) -> None:
        clean_kwargs = _sanitize_ctk_entry_kwargs(attribute)
        super().__init__(
            master,
            placeholder_text=placeholder.strip(),
            placeholder_text_color=placeholder_color,
            **clean_kwargs,
        )

    def refresh(self) -> None:
        """No-op kept for backward compatibility — CTkEntry refreshes natively."""
        return None
