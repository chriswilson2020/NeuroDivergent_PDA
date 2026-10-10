import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("flash_update", Path(__file__).resolve().parents[1] / "tools/flash_update.py")
updater = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)


class FlashUpdateTests(unittest.TestCase):
    def image(self):
        data = bytearray(64)
        data[0] = 0xE9
        data[32:36] = bytes.fromhex("3254cdab")
        return data

    def test_application_only(self):
        updater.validate_image(self.image(), "firmware.bin")
        command = updater.update_command("/dev/test", Path("firmware.bin"))
        self.assertEqual(command[-3:], ["write-flash", "0x10000", "firmware.bin"])
        self.assertNotIn("erase-flash", command)

    def test_reject_factory_even_if_renamed_bootloader(self):
        with self.assertRaises(ValueError):
            updater.validate_image(self.image(), "firmware.factory.bin")
        data = self.image()
        data[32:36] = b"\xff" * 4
        with self.assertRaises(ValueError):
            updater.validate_image(data, "renamed.bin")

    def test_reject_empty_and_oversized(self):
        for data in (b"", self.image() + bytes(updater.APP_SIZE)):
            with self.assertRaises(ValueError):
                updater.validate_image(data, "firmware.bin")
