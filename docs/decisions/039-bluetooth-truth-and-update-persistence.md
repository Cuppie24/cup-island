# 039: Who says a Bluetooth device is connected; an update offer survives a restart

Status: Accepted; the Bluetooth part superseded by 042 (Bluetooth removed)

**Context.** Two things the island showed outlived what they described:
- The Bluetooth widget kept a device after it disconnected, even after
  Bluetooth was switched off. It read only the paired-devices watcher's
  `System.Devices.Aep.IsConnected`, which Windows often doesn't update when
  a device drops, and doesn't update at all when the radio goes off.
- An update found by the daily check vanished at the next start, and came
  back only with the next check, up to a day later.

**Decision.**
- **Bluetooth: three sources, one rule** (`providers/system/bluetooth_state`,
  pure). The watcher still lists paired devices (names, pairing, its
  `IsConnected`). Each paired device's own object (`BluetoothDevice` or
  `BluetoothLEDevice`, by protocol) is opened once and its
  `ConnectionStatusChanged` followed: once that has spoken, it wins over the
  list. The Bluetooth radio (`Windows.Devices.Radios`) is followed too:
  off or disabled, nothing is connected, whatever the others say; back on,
  their word stands again. No radio found: the devices' own word stands.
  Opening a device object doesn't connect to it. All are Windows 10 1507
  APIs (decision 015). Battery watches follow what the rule says is connected.
- **An update offer is saved** in the update provider's store (`offer`: its
  version, installer link, checksum, file name and page, a line each) when
  one is found, and restored at start, before any check: the Update widget
  and its dots are back at once, without a second alert (the announced
  version is already saved). Forgotten when this build is that version or
  newer (it was installed) or when a check finds it withdrawn. Closing the
  app doesn't forget it.

**Consequences.** The rule in `tests/system_tests.cpp`, the offer's restart
in `tests/update_tests.cpp`. The WinRT side (device objects, the radio)
needs a Windows run with a real device.
