# 159: Copy logs puts a .txt file on the clipboard

Status: Accepted (amends 013). Windows-only code: needs a build and a paste on Windows.

**Context.** Copy logs put the log's last 64 KB on the clipboard as text.
Pasted into a GitHub issue, Telegram or Discord, that is a wall of lines in
the message box: Discord refuses it past 2000 characters, and the others
bury the report under it. What a bug report wants is the log attached.

**Decision.** `copyLogToClipboard` writes the same text (path header, then
the tail, decision 144's old-log end included) to
`%LOCALAPPDATA%\CupIsland\logs\cup-island-log.txt`, UTF-8 with a BOM so
Notepad reads it right, and puts that file on the clipboard as `CF_HDROP`
with a `Preferred DropEffect` of copy. No `CF_UNICODETEXT` alongside: apps
that see both paste the text, which is what this replaces.

**Consequences.**
- Paste in a chat, an issue or Explorer gives `cup-island-log.txt`. Paste
  into a plain text box (Notepad) gives nothing; open the file instead.
- The file is overwritten at each copy, so the folder holds one, not one
  per click. It stays there after the paste: the clipboard names it, so it
  can't be a temporary file deleted at once.
- The button and its notes are unchanged: a failed write or clipboard is
  "Couldn't copy logs".
