# 164: Bluetooth back, read only from Windows' device records

Status: Accepted (supersedes 042; replaces the Bluetooth part of 039). Core tested; needs a Windows run with music playing over Bluetooth headphones.

**Context.** Bluetooth was removed (decision 042) because music on
Bluetooth headphones stuttered while the island ran. The old widget used
four sources (decision 039): an association-endpoint watcher over paired
devices, each paired device's `BluetoothDevice` / `BluetoothLEDevice`
object, the radio, and battery watches over device nodes. The first two
reach the radio: an association-endpoint watcher for Bluetooth runs an
inquiry, which takes the radio's slots from the audio stream, and a
device object can bring up a link. Stopping the watcher after each
listing (tried before 042) still ran an inquiry every 10 minutes and
whenever the radio came back on. The user asked for Bluetooth back, done
the right way.

**Decision.**
- **Only what Windows has written down.** One watcher over Bluetooth's
  device nodes (`DeviceInformationKind::Device`, enumerators `BTHENUM`
  and `BTHLE`): Plug and Play's records, which Windows keeps up to date
  itself. From them: the container (one device's nodes), the device's
  name (from its own `\DEV_` node, not a service's), the "is connected"
  record (`{83DA6326-97A6-4088-9453-A1923F573B29} 15`) and the battery
  level (`DEVPKEY_Bluetooth_Battery`, the one Settings shows). The
  watcher stays running: it is told of changes, it doesn't ask devices.
- **Never** an association-endpoint watcher, `FindAllAsync` over
  association endpoints, or a `BluetoothDevice` / `BluetoothLEDevice`
  object. No timed re-listing.
- **The radio** (`Windows.Devices.Radios`): its state is local. Off,
  nothing is connected whatever the records still say; none, no widget.
  The switch sets it (`RequestAccessAsync`, `SetStateAsync`): radio
  traffic only because the user asked for it.
- **The rule is pure** (`providers/system/bluetooth_state`): a device is
  connected when any of its nodes is, and the radio isn't off; shown in
  the order they connected, with the best level any node reports.
- **The widget** (`bluetooth_panel`, wide), always two lines so it never
  changes height: one device, its name over its battery; two, a line each
  with the battery at its end; three and more, how many over their
  batteries side by side (the names are on the card). On, nothing
  connected: "No devices connected". Off: "Bluetooth is off / Turn on to
  see your devices", never a dimmed list (dimmed devices still read as
  connected). The radio's switch is its one control, in the middle of
  the column (decision 163).
- **The card**: the switch at the top, then each device with its level
  as a gauge and the number.
- **Battery colour is text only** (`Text::tone`): above
  `batteryGoodLevel` (70) good, above `batteryLowLevel` (20) fair, else
  low; `tone*Color` on a dark island, `tone*LightColor` on a pale one.
  Rows and tiles stay neutral; the card's gauge is the one exception, a
  gauge rather than a surface.
- **Logged**: every reading (`bluetooth: node ...`), so a Windows run shows
  what each record says.

**Consequences.** Tested in `tests/bluetooth_tests.cpp`. The WinRT side
needs a Windows run, as 042 asked: music on Bluetooth headphones for a few
minutes with the island idle, with Home open, and around flipping the
switch, with no stutter; and the log's `connected=` matching Settings. If
the "is connected" record turns out not to follow a device on some
driver, the fix is another record, never a device object.
