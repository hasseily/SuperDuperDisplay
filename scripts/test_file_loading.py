#!/usr/bin/env python3
"""Regression checks for sample assets, VCR layout, and forced file formats."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]


def require(text: str, marker: str, source: str) -> None:
    if marker not in text:
        raise AssertionError(f"missing {marker!r} in {source}")


def check_pal256_samples() -> None:
    samples = (
        ("observatory.pal256", 0x8000, 0),
        ("skycity.pal256i", 0x10000, 1),
    )
    for name, expected_size, paging_mode in samples:
        data = (ROOT / "samples" / name).read_bytes()
        assert len(data) == expected_size, (name, len(data), expected_size)
        assert data[0x7DFC:0x7E00] == bytes((0xD3, 0xC8, 0xD2, 0xB4)), name
        assert data[0x7DF8] == paging_mode, name


def check_karateka_vcr() -> None:
    path = ROOT / "recordings" / "karateka.vcr"
    with path.open("rb") as recording:
        header = recording.read(28)
    version, cycles_total, _cycle_current, snapshot_interval, event_count = struct.unpack(
        "<IIIQQ", header
    )
    assert version == 1
    assert cycles_total in (17030, 20280)
    assert snapshot_interval > 0
    snapshot_count = (event_count - 1) // snapshot_interval + 1
    expected_size = 28 + snapshot_count * (128 * 1024) + event_count * 6
    assert path.stat().st_size == expected_size


def check_vcr_header_layouts() -> None:
    """Exercise the discriminator used by the C++ reader with both layouts."""
    versioned = struct.pack("<IIIQQ", 2, 17030, 123, 1_000_000, 42)
    legacy = struct.pack("<QQ", 1_000_000, 42)

    version, cycles_total = struct.unpack_from("<II", versioned)
    assert version in (1, 2) and cycles_total in (17030, 20280)
    snapshot_interval, event_count = struct.unpack_from("<QQ", versioned, 12)
    assert (snapshot_interval, event_count) == (1_000_000, 42)

    legacy_first_word, legacy_second_word = struct.unpack_from("<II", legacy)
    assert legacy_first_word == 1_000_000
    assert legacy_second_word not in (17030, 20280)
    snapshot_interval, event_count = struct.unpack_from("<QQ", legacy)
    assert (snapshot_interval, event_count) == (1_000_000, 42)


def check_source_contracts() -> None:
    loader = (ROOT / "extras" / "MemoryLoader.cpp").read_text(encoding="utf-8")
    video = (ROOT / "A2VideoManager.cpp").read_text(encoding="utf-8")
    recorder = (ROOT / "EventRecorder.cpp").read_text(encoding="utf-8")
    menu = (ROOT / "MainMenu.cpp").read_text(encoding="utf-8")
    main = (ROOT / "main.cpp").read_text(encoding="utf-8")

    for extension in (".pal256", ".pal256i"):
        require(loader, extension, "extras/MemoryLoader.cpp")
    for mode in (
        "TEXT", "DTEXT", "LGR", "DLGR", "HGR", "DHGR160", "SHR3200",
        "SHR4_RGGB", "SHR4_PAL256", "SHR4_PAL256I", "SHR4_R4G4B4",
    ):
        require(loader + video, f"MemoryLoadFormat_e::{mode}", "forced format mapping")
    require(video, "GetMemoryLoadStart(loadFormat)", "A2VideoManager.cpp")
    require(video, "overrideDoubleSHR = DOUBLE_INTERLACE + 1", "A2VideoManager.cpp")

    require(recorder, "snapshot_index >= v_memSnapshots.size()", "EventRecorder.cpp")
    require(recorder, "v_events.reserve(_size)", "EventRecorder.cpp")
    require(recorder, "VCR_EVENT_BYTES_V1 = 6", "EventRecorder.cpp")
    require(recorder, "Loading legacy unversioned VCR recording", "EventRecorder.cpp")
    require(recorder, "const bool hasVersionedHeader", "EventRecorder.cpp")
    require(loader, "Error! Unable to open SHR file", "extras/MemoryLoader.cpp")
    require(main, "GetModuleFileNameW", "main.cpp")
    for resource_directory in ("assets", "shaders", "samples", "recordings"):
        require(main, f'directory / "{resource_directory}"', "main.cpp")
    require(main, "std::filesystem::current_path(workingDirectory", "main.cpp")
    require(menu, "shrLoaded && legacyLoaded", "MainMenu.cpp")
    require(menu, "if (!replayStarted)", "MainMenu.cpp")
    require(menu, "PAL256 (Observatory)", "MainMenu.cpp")
    require(menu, "PAL256i (Sky City)", "MainMenu.cpp")
    require(video, "beamState = outerBeamState", "A2VideoManager.cpp")


def main() -> None:
    check_pal256_samples()
    check_karateka_vcr()
    check_vcr_header_layouts()
    check_source_contracts()
    print("File loading and sample checks passed")


if __name__ == "__main__":
    main()
