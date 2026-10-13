# 042: No Bluetooth features

Status: Accepted (supersedes the Bluetooth parts of 009 and 039)

**Context.** Music stuttered while Cup Island ran. The Bluetooth widget
(Home's connected device and its battery) kept a Windows device watcher over
paired Bluetooth devices running for the app's whole life, opened every
paired device's object, and followed the radio. A running Bluetooth watcher
keeps the radio busy, and the radio shares its airtime with the audio
stream, so music on Bluetooth headphones stutters. Stopping the watcher
after one listing would have helped, but the widget only showed a name and
a battery level: not worth any risk to the music the island is mostly about.

**Decision.** Bluetooth is removed entirely: the provider, its connection
rules (`bluetooth_state`), the widget's presenter and panel, the glyph and
its tests. Cup Island opens no Bluetooth or radio APIs at all. The theme
toggle and Settings stay as Home's system widgets.

**Consequences.** Home has no static widget (no card, no tap action) for
now; the rules for them stay (decision 009, keyboard stops in 033), tested
with plain panels. If a Bluetooth feature comes back, it must not leave a
device watcher running, and it needs a Windows test with music playing
over Bluetooth headphones.
