// Shared declarations for the web_setting.cpp split (PR-3.1).
//
// web_setting.cpp keeps the page chrome (init_page_header /
// init_page_footer / HomePage), i18n table (getText / setLanguage /
// getLangParam), the Send_HTML wrapper, and the global webpage
// buffers. The split TUs (web_setting_forms.cpp + web_setting_handlers.cpp)
// each include this header to reach the shared state without dragging
// the full web_setting.h public API.
//
// public web_setting.h still exposes every exported function the
// server.cpp router binds to; this internal header is purely for
// cross-TU plumbing inside the server/ subdir.

#ifndef WEB_SETTING_INTERNAL_H
#define WEB_SETTING_INTERNAL_H

#include <WString.h>

extern String webpage;
extern String webpage_header;  // populated by init_page_header(); File_Upload + handleFileUpload reuse it directly
extern String webpage_footer;  // populated by init_page_footer(); same callers as above

// Send `content` wrapped in the localised page header / footer.
void Send_HTML(const String &content);

// i18n: look up a UI label by key in the current language.
const char *getText(const char *key);

// Build a "?lang=..." query-string snippet preserving the current
// language for redirect-style links.
String getLangParam();

#endif
