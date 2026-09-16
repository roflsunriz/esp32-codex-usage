import importlib.util
import re
import tempfile
from pathlib import Path
import sys
import unittest


spec = importlib.util.spec_from_file_location(
    "embed_web", Path(__file__).resolve().parents[1] / "scripts/embed-web.py"
)
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def extract_parts(header: str) -> list:
    return re.findall(r'R"ESP32SP_[0-9a-f]{8}\((.*?)\)ESP32SP_[0-9a-f]{8}"', header, re.S)


class EmbedWebTests(unittest.TestCase):
    def test_split_reconstructs_page_with_nonce(self):
        source = "<html>'__SETUP_NONCE__'</html>\n"
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "setup.html"
            dst = Path(tmp) / "setup-page.h"
            src.write_text(source, encoding="utf-8")
            module.embed(src, dst)
            parts = extract_parts(dst.read_text(encoding="utf-8"))
        self.assertEqual(len(parts), 2)
        self.assertEqual(parts[0] + "abc123" + parts[1], "\n" + source.replace("__SETUP_NONCE__", "abc123"))

    def test_split_uses_first_placeholder_only(self):
        source = "a__SETUP_NONCE__b__SETUP_NONCE__c\n"
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "setup.html"
            dst = Path(tmp) / "setup-page.h"
            src.write_text(source, encoding="utf-8")
            module.embed(src, dst)
            parts = extract_parts(dst.read_text(encoding="utf-8"))
        self.assertEqual(len(parts), 2)
        self.assertEqual(parts[0] + "N" + parts[1], "\n" + source.replace("__SETUP_NONCE__", "N", 1))

    def test_missing_placeholder_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "setup.html"
            dst = Path(tmp) / "setup-page.h"
            src.write_text("<html>no nonce</html>\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                module.embed(src, dst)


if __name__ == "__main__":
    unittest.main()
