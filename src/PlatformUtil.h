#pragma once

#include <string>

// Opens url in the OS's default handler (browser for http(s), mail client for mailto:, etc.) - shared by
// MarkdownView::open_url() (clicked links/images) and AboutView::open_url() (About dialog links), plus the
// "Check for Updates" menu item which just opens a fixed GitHub URL directly. No-op if url is empty.
void OpenExternalUrl(const std::string& url);
