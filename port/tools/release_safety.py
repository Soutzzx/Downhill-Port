"""Inspect a locally built Windows EXE without ever executing it.

Verifies PE VERSIONINFO against version.h and reports SHA-256. An optional
explicit --defender switch runs the installed Windows Defender file scan.
It never disables antivirus, adds exclusions, or treats an unsigned build
as proof of malware or of safety.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys


def expected_version():
    header = (Path(__file__).resolve().parents[1] / "include/version.h").read_text(encoding="utf-8")
    m = re.search(r'#define\s+DHD_VERSION\s+"(\d+)\.(\d+)\.(\d+)"', header)
    if not m:
        raise RuntimeError("Cannot read DHD_VERSION in version.h")
    return tuple(map(int, m.groups())) + (0,)


class FixedInfo(ctypes.Structure):
    _fields_ = [(name, wintypes.DWORD) for name in (
        "signature", "struct_version", "file_version_ms", "file_version_ls",
        "product_version_ms", "product_version_ls", "flags_mask", "flags",
        "file_os", "file_type", "file_subtype", "file_date_ms", "file_date_ls"
    )]


def pe_version(path):
    if os.name != "nt":
        raise RuntimeError("Windows is required to read PE VERSIONINFO")
    version = ctypes.WinDLL("version", use_last_error=True)
    version.GetFileVersionInfoSizeW.argtypes = (wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD))
    version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    version.GetFileVersionInfoW.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p)
    version.GetFileVersionInfoW.restype = wintypes.BOOL
    version.VerQueryValueW.argtypes = (
        ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(wintypes.UINT))
    version.VerQueryValueW.restype = wintypes.BOOL
    dummy = wintypes.DWORD()
    size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(dummy))
    if not size:
        raise RuntimeError("Missing Windows VERSIONINFO metadata")
    data = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, data):
        raise RuntimeError("Could not read Windows VERSIONINFO metadata")
    ptr = ctypes.c_void_p()
    length = wintypes.UINT()
    if not version.VerQueryValueW(data, "\\", ctypes.byref(ptr), ctypes.byref(length)):
        raise RuntimeError("Invalid Windows VERSIONINFO metadata")
    if length.value < ctypes.sizeof(FixedInfo):
        raise RuntimeError("Truncated Windows VERSIONINFO metadata")
    info = ctypes.cast(ptr, ctypes.POINTER(FixedInfo)).contents
    if info.signature != 0xFEEF04BD:
        raise RuntimeError("Invalid VS_FIXEDFILEINFO signature")
    return (info.file_version_ms >> 16, info.file_version_ms & 65535,
            info.file_version_ls >> 16, info.file_version_ls & 65535)


def check(path):
    path = path.resolve()
    if not path.is_file():
        raise RuntimeError("No compiled executable at " + str(path))
    with path.open("rb") as f:
        if f.read(2) != b"MZ":
            raise RuntimeError("Output is not a Windows PE executable")
    version = pe_version(path)
    expected = expected_version()
    if version != expected:
        raise RuntimeError(f"Built version {version} does not match version.h {expected}")
    digest = hashlib.file_digest(path.open("rb"), "sha256").hexdigest()
    print(f"Executable: {path}")
    print(f"Version: {'.'.join(map(str, version))}")
    print(f"SHA-256: {digest}")
    print("Windows version metadata: OK")
    print("Note: this is not a malware verdict or digital signature.")
    return path


def defender(path):
    candidates = [Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Windows Defender" / "MpCmdRun.exe"]
    platform = Path(os.environ.get("ProgramData", r"C:\ProgramData")) / "Microsoft" / "Windows Defender" / "Platform"
    if platform.is_dir():
        candidates = sorted(platform.glob("*/MpCmdRun.exe"), reverse=True) + candidates
    scanner = next((p for p in candidates if p.is_file()), None)
    if not scanner:
        raise RuntimeError("Microsoft Defender command-line scanner is not installed")
    print(f"Scanning {path} with Microsoft Defender; detections may quarantine the file.", flush=True)
    result = subprocess.run([str(scanner), "-Scan", "-ScanType", "3", "-File", str(path)],
                            timeout=240, check=False)
    if result.returncode != 0 or not path.is_file():
        raise RuntimeError(
            f"Defender could not clear this build (scanner exit {result.returncode}). "
            "Check Windows Security > Protection history; do not distribute it."
        )
    print("Defender custom-file scan finished successfully (not a multi-engine safety guarantee).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path, help="Path to compiled Windows dhd.exe")
    parser.add_argument("--defender", action="store_true",
                        help="Explicitly scan this file using local Microsoft Defender")
    args = parser.parse_args()
    try:
        path = check(args.exe)
        if args.defender:
            defender(path)
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as exc:
        print("RELEASE CHECK FAILED:", exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
