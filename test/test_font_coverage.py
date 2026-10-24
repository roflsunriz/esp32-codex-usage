import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class FontCoverageTest(unittest.TestCase):
    def test_every_non_ascii_source_character_has_a_glyph(self) -> None:
        source_text = "".join(
            path.read_text(encoding="utf-8")
            for folder in (ROOT / "src", ROOT / "include")
            for path in folder.glob("*.*")
            if path.name != "japanese-font.h" and path.suffix in {".cpp", ".h"}
        )
        literals = "".join(re.findall(r'"(?:\\.|[^"\\])*"', source_text))
        needed = {ord(character) for character in literals if ord(character) > 127}
        generated = (ROOT / "include" / "japanese-font.h").read_text(
            encoding="utf-8"
        )
        available = {
            int(value, 16)
            for value in re.findall(r"\{0x([0-9A-F]{4}),", generated)
        }
        self.assertEqual(set(), needed - available)


if __name__ == "__main__":
    unittest.main()
