#!/usr/bin/env python3
"""Source-level and mapping checks for the packed PAL256/PAL256i layout."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAL256_WIDTH = 320
PAL256_HEIGHT = 100
SHR_BYTES_PER_LINE = 160


def packed_coordinate(source_y: int, source_x: int) -> tuple[int, int]:
    return source_y // 2, source_x + (source_y % 2) * SHR_BYTES_PER_LINE


def presentation_coordinate(
    output_x: int, output_y: int, page_mode: int, frame_is_odd: int = 0
) -> tuple[str, int, int]:
    packed_x = output_x // 2
    if page_mode == 1:  # PAL256i: fixed vertical AUX/main composition
        combined_y = output_y // 2
        bank = "aux" if combined_y < PAL256_HEIGHT else "main"
        return bank, packed_x, combined_y % PAL256_HEIGHT

    bank = "main" if page_mode == 2 and frame_is_odd else "aux"
    return bank, packed_x, output_y // 4


def check_contiguous_repack() -> None:
    for source_y in range(200):
        for source_x in range(SHR_BYTES_PER_LINE):
            packed_y, packed_x = packed_coordinate(source_y, source_x)
            source_offset = source_y * SHR_BYTES_PER_LINE + source_x
            packed_offset = packed_y * PAL256_WIDTH + packed_x
            assert source_offset == packed_offset

    assert packed_coordinate(0, 0) == (0, 0)
    assert packed_coordinate(0, 159) == (0, 159)
    assert packed_coordinate(1, 0) == (0, 160)
    assert packed_coordinate(1, 159) == (0, 319)
    assert packed_coordinate(199, 159) == (99, 319)


def check_presentation_mapping() -> None:
    assert presentation_coordinate(0, 0, 0) == ("aux", 0, 0)
    assert presentation_coordinate(639, 399, 0) == ("aux", 319, 99)

    # PAL256i boundary: the complete AUX field is above the main field.
    assert presentation_coordinate(0, 199, 1) == ("aux", 0, 99)
    assert presentation_coordinate(0, 200, 1) == ("main", 0, 0)
    assert presentation_coordinate(639, 399, 1) == ("main", 319, 99)

    # Page flip remains temporal and selects a complete packed field.
    assert presentation_coordinate(0, 0, 2, 0) == ("aux", 0, 0)
    assert presentation_coordinate(0, 0, 2, 1) == ("main", 0, 0)


def check_source_contract() -> None:
    common = (ROOT / "common.h").read_text(encoding="utf-8")
    beam = (ROOT / "A2WindowBeam.cpp").read_text(encoding="utf-8")
    shader = (ROOT / "shaders" / "a2video_beam_shr_raw.frag").read_text(
        encoding="utf-8"
    )
    spec = (ROOT / "docs" / "SHR3200_SHR4_VIDEO_MODES.md").read_text(
        encoding="utf-8"
    )
    workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
        encoding="utf-8"
    )

    required_common = (
        "#define _A2VIDEO_PAL256_WIDTH (_A2VIDEO_SHR_BYTES_PER_LINE*2)",
        "#define _A2VIDEO_PAL256_HEIGHT (_A2VIDEO_SHR_SCANLINES/2)",
    )
    for marker in required_common:
        assert marker in common

    required_beam = (
        "_A2VIDEO_PAL256_WIDTH, _A2VIDEO_PAL256_HEIGHT * (_hasDSHR4 + 1)",
        "_hasDSHR4 * _A2VIDEO_PAL256_HEIGHT",
    )
    for marker in required_beam:
        assert marker in beam

    required_shader = (
        "if ((specialModesMask & A2SM_SHR4PAL256) != 0)",
        "pal256Y = ypos_noborder >> 1;",
        "pal256Y = ypos_noborder >> 2;",
        "ivec2(xpos_noborder >> 1, pal256Y)",
    )
    for marker in required_shader:
        assert marker in shader

    assert "The native PAL256 raster is 320×100" in spec
    assert "PAL256i is one 320×200 image" in spec
    assert "160×200 in single-page mode" not in spec
    assert "160×400 in interlace mode" not in spec
    assert '#define SDD_VERSION "0.9.1 \\"PAL256\\""' in common
    assert "SDD_VERSION: 0.9.1" in workflow


def main() -> None:
    check_contiguous_repack()
    check_presentation_mapping()
    check_source_contract()
    print("PAL256 layout checks passed")


if __name__ == "__main__":
    main()
