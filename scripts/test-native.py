"""Build and run the native Unity tests without PlatformIO's host toolchain."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


ROOT = Path(__file__).resolve().parents[1]
UNITY_ROOT = ROOT / ".pio" / "libdeps" / "native" / "Unity"
UNITY_SOURCE = UNITY_ROOT / "src" / "unity.c"
ARDUINOJSON_INCLUDE = (
    ROOT / ".pio" / "libdeps" / "native" / "ArduinoJson" / "src"
)
NATIVE_OUTPUT = ROOT / ".local" / "native-tests"


def _existing_file(candidates: Iterable[Path]) -> Optional[Path]:
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def _compiler_pair() -> Tuple[Path, Path]:
    """Return C++ and C compilers, preferring the available native toolchain."""
    configured_cxx = os.environ.get("CXX")
    configured_cc = os.environ.get("CC")

    if configured_cxx:
        cxx = Path(configured_cxx)
        if not cxx.is_file():
            cxx = Path(shutil.which(configured_cxx) or configured_cxx)
    elif os.name == "nt":
        program_files = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
        cxx = _existing_file(
            [
                program_files / "LLVM" / "bin" / "clang++.exe",
                Path(shutil.which("clang++") or "clang++"),
                Path(shutil.which("g++") or "g++"),
            ]
        ) or Path("clang++")
    else:
        cxx = _existing_file(
            [
                Path(shutil.which("g++") or "g++"),
                Path(shutil.which("clang++") or "clang++"),
            ]
        ) or Path("g++")

    if configured_cc:
        cc = Path(configured_cc)
        if not cc.is_file():
            cc = Path(shutil.which(configured_cc) or configured_cc)
    else:
        adjacent_name = "clang.exe" if cxx.name.lower() == "clang++.exe" else "gcc"
        adjacent = cxx.with_name(adjacent_name)
        cc = _existing_file(
            [
                adjacent,
                Path(shutil.which("clang") or "clang"),
                Path(shutil.which("gcc") or "gcc"),
            ]
        ) or cxx

    return cxx, cc


def _load_visual_studio_environment(environment: Dict[str, str]) -> Dict[str, str]:
    """Load MSVC linker paths while keeping compiler calls as direct subprocesses."""
    if os.name != "nt":
        return environment

    vswhere_candidates = [
        Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"),
        Path("C:/Program Files/Microsoft Visual Studio/Installer/vswhere.exe"),
    ]
    vswhere = _existing_file(vswhere_candidates)
    devcmd: Optional[Path] = None
    if vswhere:
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode == 0:
            installation = result.stdout.strip().splitlines()
            if installation:
                devcmd = Path(installation[-1]) / "Common7" / "Tools" / "VsDevCmd.bat"

    if devcmd is None or not devcmd.is_file():
        return environment

    # The .bat file is only used to discover the linker environment. Every
    # compiler and linker invocation below still receives an argument list and
    # runs directly through subprocess.run(shell=False).
    result = subprocess.run(
        ["cmd.exe", "/d", "/s", "/c", f'call "{devcmd}" -arch=amd64 >nul && set'],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        return environment

    loaded = dict(environment)
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if separator and key:
            loaded[key] = value
    return loaded


def _fallback_unity_config(output: Path) -> Path:
    """Create the small Unity host adapter if PlatformIO has not generated it."""
    config = output / "unity_config"
    config.mkdir(parents=True, exist_ok=True)
    header = config / "unity_config.h"
    source = config / "unity_config.c"
    if not header.exists():
        header.write_text(
            """#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif
void unityOutputStart(unsigned long);
void unityOutputChar(unsigned int);
void unityOutputFlush(void);
void unityOutputComplete(void);
#define UNITY_OUTPUT_START() unityOutputStart(115200UL)
#define UNITY_OUTPUT_CHAR(c) unityOutputChar((unsigned int)(c))
#define UNITY_OUTPUT_FLUSH() unityOutputFlush()
#define UNITY_OUTPUT_COMPLETE() unityOutputComplete()
#ifdef __cplusplus
}
#endif
#endif
""",
            encoding="utf-8",
        )
    if not source.exists():
        source.write_text(
            """#include <stdio.h>
#include <unity_config.h>
#if defined(__GNUC__) || defined(__clang__)
#define UNITY_NATIVE_WEAK __attribute__((weak))
#else
#define UNITY_NATIVE_WEAK
#endif
UNITY_NATIVE_WEAK void setUp(void) {}
UNITY_NATIVE_WEAK void tearDown(void) {}
UNITY_NATIVE_WEAK void suiteSetUp(void) {}
UNITY_NATIVE_WEAK int suiteTearDown(int failures) { return failures; }
void unityOutputStart(unsigned long baudrate) { (void)baudrate; }
void unityOutputChar(unsigned int c) { putchar((int)c); }
void unityOutputFlush(void) { fflush(stdout); }
void unityOutputComplete(void) {}
""",
            encoding="utf-8",
        )
    return config


def _unity_config_dir(output: Path) -> Tuple[Path, Path]:
    generated = ROOT / ".pio" / "build" / "native" / "unity_config"
    generated_header = generated / "unity_config.h"
    generated_source = generated / "unity_config.c"
    if generated_header.is_file() and generated_source.is_file():
        return generated, generated_source
    fallback = _fallback_unity_config(output)
    return fallback, fallback / "unity_config.c"


def _run(command: Sequence[str], environment: Dict[str, str]) -> int:
    print("[native] " + " ".join(command), flush=True)
    completed = subprocess.run(
        list(command),
        cwd=str(ROOT),
        env=environment,
        check=False,
    )
    return completed.returncode


def _compile_flags(config_dir: Path) -> List[str]:
    return [
        "-DUNITY_INCLUDE_CONFIG_H",
        "-DUNITY_SUPPORT_64",
        "-DUNITY_INCLUDE_DOUBLE",
        "-DUNITY_WEAK_ATTRIBUTE=__attribute__((weak))",
        "-I",
        str(ROOT / "include"),
        "-I",
        str(UNITY_ROOT / "src"),
        "-I",
        str(config_dir),
        "-I",
        str(ARDUINOJSON_INCLUDE),
    ]


def _build_and_run(test_sources: Sequence[Path]) -> int:
    if not UNITY_SOURCE.is_file():
        print(f"Unity source is missing: {UNITY_SOURCE}", file=sys.stderr)
        return 2
    if not ARDUINOJSON_INCLUDE.is_dir():
        print(f"ArduinoJson headers are missing: {ARDUINOJSON_INCLUDE}", file=sys.stderr)
        return 2

    cxx, cc = _compiler_pair()
    environment = _load_visual_studio_environment(dict(os.environ))
    NATIVE_OUTPUT.mkdir(parents=True, exist_ok=True)
    config_dir, config_source = _unity_config_dir(NATIVE_OUTPUT)
    flags = _compile_flags(config_dir)

    unity_object = NATIVE_OUTPUT / "unity.o"
    config_object = NATIVE_OUTPUT / "unity_config.o"
    c_flags = [str(cc), "-std=c11", *flags]
    if cc == cxx:
        c_flags.insert(1, "-x")
        c_flags.insert(2, "c")

    if _run([*c_flags, "-c", str(UNITY_SOURCE), "-o", str(unity_object)], environment):
        return 1
    if _run([*c_flags, "-c", str(config_source), "-o", str(config_object)], environment):
        return 1

    failures = 0
    for source in test_sources:
        test_name = source.parent.name
        test_object = NATIVE_OUTPUT / f"{test_name}.o"
        executable_name = test_name + (".exe" if os.name == "nt" else "")
        executable = NATIVE_OUTPUT / executable_name
        # VS 18's STL uses C++14 constructs even when the project flag is
        # C++11. The state header remains C++11-compatible; use the newer
        # dialect only for these host-side test translation units.
        standard = "-std=c++17" if os.name == "nt" else "-std=c++11"
        cpp_flags = [str(cxx), standard, *flags]
        if _run([*cpp_flags, "-c", str(source), "-o", str(test_object)], environment):
            failures += 1
            continue

        link = [
            str(cxx),
            str(unity_object),
            str(config_object),
            str(test_object),
            "-o",
            str(executable),
        ]
        if os.name != "nt":
            link.append("-lm")
        if _run(link, environment):
            failures += 1
            continue

        result = _run([str(executable)], environment)
        if result:
            failures += 1

    return 1 if failures else 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "tests",
        nargs="*",
        help="optional test directory names (default: every test/*/test_main.cpp)",
    )
    args = parser.parse_args(argv)

    all_sources = sorted((ROOT / "test").glob("*/test_main.cpp"))
    if args.tests:
        requested = set(args.tests)
        all_sources = [source for source in all_sources if source.parent.name in requested]
    if not all_sources:
        print("No native test sources were found.", file=sys.stderr)
        return 2
    return _build_and_run(all_sources)


if __name__ == "__main__":
    raise SystemExit(main())
