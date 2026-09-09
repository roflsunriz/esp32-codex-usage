import errno
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location(
    "setup_ui_cleanup", Path(__file__).resolve().parents[1] / "scripts/test-setup-ui.py"
)
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


class ProfileCleanupTests(unittest.TestCase):
    def test_retries_transient_writer_race(self):
        profile = Mock()
        profile.cleanup.side_effect = [OSError(errno.ENOTEMPTY, "busy"), None]
        with patch.object(module.time, "sleep"):
            module.cleanup_profile(profile)
        self.assertEqual(profile.cleanup.call_count, 2)

    def test_persistent_failure_is_reported(self):
        profile = Mock()
        profile.cleanup.side_effect = OSError(errno.ENOTEMPTY, "busy")
        with patch.object(module.time, "monotonic", side_effect=[0, 6]):
            with self.assertRaises(OSError):
                module.cleanup_profile(profile)

    def test_unrelated_error_is_not_retried(self):
        profile = Mock()
        profile.cleanup.side_effect = OSError(errno.EIO, "disk error")
        with self.assertRaises(OSError):
            module.cleanup_profile(profile)
        profile.cleanup.assert_called_once()
