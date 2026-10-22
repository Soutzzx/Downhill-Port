# Keyboard controls and controller detection (Windows)

Experimental in-game UI for the native Windows port, using its existing PS2-style menu.

## Where to find it

Launch a fresh build, then go to **OPTIONS > PC SETTINGS > CONTROLS & CONTROLLERS**.

- **KEY BINDINGS**: two pages covering steering, pedals, brakes, tricks, camera, start/pause,
  sprint, punches and kicks. Select an action with Cross / Space, release the old key,
  then press a new key. **Esc cancels remapping**.
- **Restore default keys**: on the second key-bindings page.
- **CONTROLLERS**: from the second key-bindings page, showing all four SDL3 player slots,
  device names and button bit masks (BTN:XXXX). Press a button and check whether the
  value changes. SDL3 hot-plug events are automatic. If SDL3 cannot initialize,
  the page also checks XInput and supported Sony HID fallback devices.
- **Triangle / Esc**: back to previous page. Esc stays fixed and cannot be rebound.

Key bindings are saved as bind_00 through bind_18 in the dhd.ini file beside
the native game's executable. These are Windows virtual-key numbers; they are not
additional scripts. A remap that conflicts with another mapping swaps the two keys
so one press does not trigger two actions. The keys in existing installs keep their
original defaults unless changed.

The controller screen is **detection and input diagnostics**, not a complete
controller calibration/remapping tool: SDL3 uses its standard Xbox/PlayStation/
Switch mapping, and generic joysticks get the fallback layout described in
port/hle/gamepad.c. You can provide gamecontrollerdb.txt next to dhd.exe
to add or fix SDL3 mappings for a nonstandard controller.

## Troubleshooting

1. Connect your controller before starting the game, then open the CONTROLLERS page.
2. If the name appears and the BTN code changes when pressing buttons, the device is
   being read. If gameplay buttons are wrong, its SDL mapping likely needs adjustment.
3. If no controller appears, try Windows' controller tester (Win + R, then joy.cpl).
   Verify USB/Bluetooth connectivity. Check DHD_SDL is not explicitly set to 0
   in the process environment. SDL3 is the normal input path; DHD_SDL=0 enables
   the limited native XInput/Sony fallback.
4. A failed detection in-game does not prove the physical controller is defective.

## Implementation and limitations

- Key capture suppresses mapped keyboard input while waiting for a new key, so the
  new key does not also trigger a game action.
- SDL controller state is copied under its input-thread lock.
- The controls UI is a new view of the existing PC SETTINGS menu, not a second nested
  popup. It makes no changes to races or network synchronization.
- There is no game image, PS2 executable or retail data in this change.
- **Requires Windows build and game tests** with keyboard and controller hardware:
  GitHub code inspection alone cannot confirm the behavior of the original game's
  menu vtable, SDL controller driver, or every keyboard layout.
