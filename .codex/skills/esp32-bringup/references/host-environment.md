# Host Environment And Port Detection

Use before running OS-specific install, serial, upload, or monitor commands.

## Identify Host Style

Detect what environment the agent is running in before asking the user to identify it:

```bash
uname -a
```

Useful hints:

- Linux: normal `/dev/ttyACM*` or `/dev/ttyUSB*` devices.
- macOS: `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`.
- Windows PowerShell: `COMx` ports and Device Manager.
- Windows WSL2: Linux shell plus Windows USB devices shared through `usbipd-win`.

In WSL2, check:

```bash
grep -i microsoft /proc/version
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
command -v powershell.exe >/dev/null && powershell.exe -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"
```

If local detection gives a clear answer, state it briefly and continue. Ask the user about host type only when detection is unavailable or contradictory.

## Port Discovery

Linux or WSL:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

macOS:

```bash
ls -l /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null
```

Windows PowerShell:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

## WSL2 usbipd Guidance

If the board appears in Windows but not inside WSL2, guide the user through `usbipd-win`. Some steps require administrator rights, so ask the user to run them when the agent cannot.

First check whether Windows can see `usbipd`:

```bash
powershell.exe -NoProfile -Command "if (Get-Command usbipd -ErrorAction SilentlyContinue) { usbipd list } else { 'usbipd not found' }"
```

If `usbipd` is not installed, ask the user to install `usbipd-win` on Windows. Prefer the official installer or Windows Package Manager:

```powershell
winget install --id dorssel.usbipd-win
```

Tell the user this may require an administrator PowerShell or UAC approval.

Do not continue to upload or monitor from WSL until WSL shows a real `/dev/ttyACM*` or `/dev/ttyUSB*` device.

Windows PowerShell:

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

WSL2:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

If no Linux serial device appears after attach, ask the user to unplug/replug the board, run `usbipd list` again, and confirm the selected BUSID is still attached to the intended WSL distribution.

If permission is denied on Linux/WSL, explain that the user may need:

```bash
sudo usermod -aG dialout "$USER"
```

Then restart the shell or WSL session and reattach the USB device.

## Install Guidance

Do not pretend to complete privileged installs when permission is unavailable. Give the exact command and explain when it needs administrator or sudo rights.

Host tooling may be installed or prepared before board specs are complete, but framework-specific project files must still wait for framework confirmation and board spec lookup.

Common needs:

- PlatformIO CLI for PlatformIO projects.
- ESP-IDF install/export for `idf.py` projects.
- Python `pyserial` for bounded serial monitoring.
- `usbipd-win` on Windows when using WSL2 with USB devices.
