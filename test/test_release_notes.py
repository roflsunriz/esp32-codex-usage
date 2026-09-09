import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "scripts" / "release-notes.py"
SPEC = importlib.util.spec_from_file_location("release_notes", MODULE_PATH)
assert SPEC and SPEC.loader
release_notes = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release_notes)


class ReleaseNotesTests(unittest.TestCase):
    def test_extracts_bracketed_version_and_stops_at_next_section(self) -> None:
        changelog = """# Changelog

## [1.2.3] - 2026-09-10

### Added

- New firmware.

## [Unreleased]

- Future work.
"""
        self.assertEqual(
            release_notes.extract_release_notes(changelog, "v1.2.3"),
            "### Added\n\n- New firmware.\n",
        )

    def test_extracts_plain_v_heading(self) -> None:
        changelog = "## v2.0.0\n\n- Breaking change.\n"
        self.assertEqual(
            release_notes.extract_release_notes(changelog, "refs/tags/v2.0.0"),
            "- Breaking change.\n",
        )

    def test_missing_version_fails(self) -> None:
        with self.assertRaises(release_notes.ReleaseNotesError):
            release_notes.extract_release_notes("## [Unreleased]\n\n- Work.\n", "v1.0.0")

    def test_empty_version_fails(self) -> None:
        with self.assertRaises(release_notes.ReleaseNotesError):
            release_notes.extract_release_notes("## [1.0.0]\n\n## [Unreleased]\n", "v1.0.0")


if __name__ == "__main__":
    unittest.main()
