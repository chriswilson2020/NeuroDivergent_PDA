"""Application-only firmware update; never overwrite NVS with factory padding."""
import argparse
from pathlib import Path
import subprocess
import sys

APP_OFFSET = 0x10000
APP_SIZE = 0x300000


def validate_image(data: bytes, name: str) -> None:
    if "factory" in Path(name).name.lower():
        raise ValueError("Factory images are not safe updates: they overwrite NVS settings.")
    if len(data) < 40 or len(data) > APP_SIZE or data[0] != 0xE9:
        raise ValueError("Not a valid-sized ESP application image.")
    if data[32:36] != bytes.fromhex("3254cdab"):
        raise ValueError("ESP application descriptor missing; refusing a bootloader/merged image.")


def update_command(port: str, image: Path) -> list[str]:
    return [sys.executable, "-m", "esptool", "--port", port, "--after",
            "watchdog-reset", "write-flash", hex(APP_OFFSET), str(image)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--image", type=Path,
                        default=Path(__file__).resolve().parents[1] / ".pio/build/tlora_pager/firmware.bin")
    args = parser.parse_args()
    try:
        validate_image(args.image.read_bytes(), str(args.image))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print("Updating application at 0x10000 only; NVS, filesystem and microSD are not erased.", flush=True)
    return subprocess.call(update_command(args.port, args.image))


if __name__ == "__main__":
    raise SystemExit(main())
