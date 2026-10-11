"""Source integration checks complement the native LVGL/service tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LockBackgroundIntegrationTests(unittest.TestCase):
    def test_saved_pin_loaded_before_startup_lock(self):
        setup = (ROOT / "src/main.cpp").read_text().split("void setup() {", 1)[1].split("void loop()", 1)[0]
        self.assertLess(setup.index("passcodes.load();"), setup.index("shell.beginLock(passcodes);"))

    def test_background_loop_not_gated_by_pin(self):
        loop = (ROOT / "src/main.cpp").read_text().split("void loop() {", 1)[1]
        self.assertNotIn("shell.locked()", loop)
        for service in ("messaging", "reminders", "timerService", "lowBattery"):
            self.assertIn(f"{service}.update();", loop)

    def test_radio_service_not_gated_by_pin(self):
        radio = (ROOT / "src/core/MessagingService.cpp").read_text()
        update = radio.split("void MessagingService::update(){", 1)[1]
        self.assertNotIn("shell_->locked()", update)
        self.assertIn("onIrq()", update)
        self.assertIn("transmitPending()", update)
        self.assertIn("ACK sent", update)
        self.assertIn("ACK timeout", update)

    def test_lock_never_suspends_radio_or_cancels_deadlines(self):
        for name in ("src/ui/LockScreen.cpp", "src/core/LockPolicy.h"):
            source = (ROOT / name).read_text()
            for forbidden in ("messaging.suspend", "radio.sleep", "timerService.cancel", "reminders.cancel"):
                self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
