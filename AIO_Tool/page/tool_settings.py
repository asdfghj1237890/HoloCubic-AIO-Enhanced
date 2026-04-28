################################################################################
#
# Tool Settings Page
# Allows users to configure tool preferences like language
#
################################################################################

import tkinter as tk
from tkinter import messagebox

import customtkinter as ctk

from util.i18n import get_i18n


class ToolSettings:
    """工具設定頁（語系選擇等偏好設定）。"""

    def __init__(self, father, engine, lock=None):
        self.m_engine = engine
        self.m_father = father
        self.i18n = get_i18n()
        self.current_language = self.i18n.get_language()

        self.init_ui(self.m_father)

    def init_ui(self, father):
        """初始化設定 UI。"""
        # Main container with padding
        main_frame = ctk.CTkFrame(father, fg_color="transparent")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        # Title
        title_label = ctk.CTkLabel(
            main_frame,
            text=self.i18n.t("tool_settings_title"),
            font=ctk.CTkFont(family="Arial", size=16, weight="bold"),
        )
        title_label.pack(pady=(0, 20))

        # Language settings section
        self.create_language_section(main_frame)

        # Restart tip
        tip_frame = ctk.CTkFrame(main_frame, fg_color="transparent")
        tip_frame.pack(fill=tk.X, pady=(20, 0))

        tip_label = ctk.CTkLabel(
            tip_frame,
            text=self.i18n.t("restart_tip"),
            font=ctk.CTkFont(family="Arial", size=11),
            text_color="gray60",
            wraplength=600,
            justify=tk.LEFT,
        )
        tip_label.pack(anchor=tk.W)

    def create_language_section(self, parent):
        """建立語系選擇區塊。"""
        # CTk 沒有 LabelFrame，用 CTkFrame + 標題 CTkLabel 模擬
        section_frame = ctk.CTkFrame(parent)
        section_frame.pack(fill=tk.X, pady=(0, 10), padx=2)

        section_title = ctk.CTkLabel(
            section_frame,
            text=self.i18n.t("language_label"),
            font=ctk.CTkFont(family="Arial", size=12, weight="bold"),
        )
        section_title.pack(anchor=tk.W, padx=15, pady=(12, 4))

        # Language tip
        tip_label = ctk.CTkLabel(
            section_frame,
            text=self.i18n.t("language_tip"),
            font=ctk.CTkFont(family="Arial", size=11),
            text_color="gray60",
        )
        tip_label.pack(anchor=tk.W, padx=15, pady=(0, 10))

        # Radio buttons for language selection
        self.language_var = tk.StringVar(value=self.current_language)

        radio_frame = ctk.CTkFrame(section_frame, fg_color="transparent")
        radio_frame.pack(fill=tk.X, padx=15)

        # Get available languages
        available_languages = self.i18n.get_available_languages()

        for lang_code, lang_name in available_languages:
            rb = ctk.CTkRadioButton(
                radio_frame,
                text=lang_name,
                variable=self.language_var,
                value=lang_code,
                font=ctk.CTkFont(family="Arial", size=12),
                command=self.on_language_change,
            )
            rb.pack(anchor=tk.W, pady=5)

        # Apply button
        button_frame = ctk.CTkFrame(section_frame, fg_color="transparent")
        button_frame.pack(fill=tk.X, padx=15, pady=(15, 12))

        self.apply_button = ctk.CTkButton(
            button_frame,
            text=self.i18n.t("apply_button"),
            font=ctk.CTkFont(family="Arial", size=12),
            fg_color="#4CAF50",
            hover_color="#45a049",
            text_color="white",
            cursor="hand2",
            command=self.apply_language_change,
        )
        self.apply_button.pack(side=tk.LEFT)

        # Initially disable the apply button
        self.apply_button.configure(state=tk.DISABLED)

    def on_language_change(self):
        """當使用者切換語系 radio 時，依照是否異動啟用 Apply 鍵。"""
        selected_language = self.language_var.get()
        if selected_language != self.current_language:
            self.apply_button.configure(state=tk.NORMAL)
        else:
            self.apply_button.configure(state=tk.DISABLED)

    def apply_language_change(self):
        """套用使用者選擇的語系。"""
        selected_language = self.language_var.get()

        if selected_language == self.current_language:
            return

        if self.i18n.save_language_preference(selected_language):
            self.i18n.set_language(selected_language)
            self.current_language = selected_language

            messagebox.showinfo(
                self.i18n.t("success_title"),
                self.i18n.t("language_changed"),
            )

            self.apply_button.configure(state=tk.DISABLED)
        else:
            messagebox.showerror(
                "Error",
                "Failed to save language preference",
            )

    def __del__(self):
        pass
