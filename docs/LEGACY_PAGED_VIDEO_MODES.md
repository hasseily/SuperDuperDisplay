# Legacy Paged Video Modes: Interlace and Page Flip

Implementation specification, revision 1.0 (2026-08-03)

## 1. Purpose and scope

This document specifies the two-page interlace and page-flip extensions for the classic Apple II video modes — LGR, DLGR, HGR, and DHGR — mirroring the `page_mode` concept that `SHR3200_SHR4_VIDEO_MODES.md` section 10 defines for SHR. It covers:

- the legacy two-page memory layout;
- the `A2Li` in-band signal, carried in page 2's first screen hole, that declares a page pair and its paged type;
- interlace (spatial) presentation, mode `1`;
- page flip (temporal) presentation, mode `2`, including the mandatory frame-merge behavior on displays slower than 120 Hz; and
- file conventions for distributing paged legacy images.

The intended audience is authors of Apple II software, video capture devices, emulators, display cards, converters, and standalone image viewers.

The normative words **MUST**, **MUST NOT**, **SHOULD**, and **MAY** describe interoperability requirements. Sections labeled "reference behavior" describe a specific current implementation — SuperDuperDisplay (SDD) or the Appletini ONE renderer — including behavior another implementation need not reproduce.

Unlike SHR, the legacy modes are defined by the machine, not by this specification. Everything here layers on top of ordinary Apple II video: an implementation needs normal LGR/DLGR/HGR/DHGR renderers as its foundation, and single-page behavior is exactly the machine's.

## 2. Legacy page pairs

Each legacy graphics mode has two hardware pages of identical layout:

| Mode | Page 1 | Page 2 | Page size | Rows per page |
| --- | --- | --- | ---: | ---: |
| LGR | main `$0400-$07FF` | main `$0800-$0BFF` | 1 KB | 48 |
| DLGR | aux + main `$0400-$07FF` | aux + main `$0800-$0BFF` | 2 KB | 48 |
| HGR | main `$2000-$3FFF` | main `$4000-$5FFF` | 8 KB | 192 |
| DHGR | aux + main `$2000-$3FFF` | aux + main `$4000-$5FFF` | 16 KB | 192 |

Row addressing within a page is the machine's interleaved layout; this document does not restate it. "Rows" means the mode's natural rows: 48 block rows for the lores family, 192 scanlines for the hires family.

The machine displays one page at a time, selected by the `PAGE2` soft switch (`$C054/$C055`). When `80STORE` (`$C000/$C001`) is on, `PAGE2` switches memory banking instead of the displayed page; a consumer that resolves "which page is live" from soft switches MUST treat `PAGE2` as a display selector only while `80STORE` is off.

The paged modes in this document use **both** pages at once. The producer fills page 1 and page 2 with the two fields (interlace) or the two temporal frames (page flip); the signal in section 3 declares the pair.

## 3. The `A2Li` in-band signal

### 3.1 Screen holes

Legacy pages contain unused bytes. In a text/lores page, each 128-byte block addresses three 40-byte rows, leaving 8 bytes unused at block offsets `$78-$7F` — the classic "screen holes." An HGR page repeats the same structure at 8× scale, so it has the same holes in every 128-byte block.

There is precedent for metadata in the first hole: the ProDOS Fotofile format (file type `$08`, inherited from Apple SOS) uses its 121st byte — page offset `+$78`, the first byte of the first hole — to declare how the image should be displayed.

`A2Li` follows that precedent, with one deliberate difference: the signal lives in the first hole of **page 2**, not page 1. Page 1's holes are avoided because in the text/lores region they are the bytes reserved as peripheral-card scratch space (`$478-$47F` and friends), which resident slot firmware overwrites at will, and because Fotofile already assigns a meaning to page 1 offset `+$78`. Page 2 carries the signal naturally: it is the page whose presence makes the image a pair.

### 3.2 Signal layout

Five consecutive bytes at page 2 offset `+$78`, in the **main** bank:

| Page-2 offset | Value | Meaning |
| --- | --- | --- |
| `+$78` | `$C1` | `'A'` with bit 7 set |
| `+$79` | `$B2` | `'2'` with bit 7 set |
| `+$7A` | `$CC` | `'L'` with bit 7 set |
| `+$7B` | `$E9` | `'i'` with bit 7 set |
| `+$7C` | mode | `0`: off; `1`: interlace; `2`: page flip |

Live addresses by mode family:

| Mode family | Signature | Mode byte |
| --- | --- | --- |
| LGR / DLGR | main `$0878-$087B` | main `$087C` |
| HGR / DHGR | main `$4078-$407B` | main `$407C` |

The four signature bytes are high-bit-set ASCII `A2Li`, compared byte-for-byte in address order. Consumers MUST require all four signature bytes and a mode byte of exactly `1` or `2`; any other combination means single-page display. Mode values `3-255` are reserved and MUST be treated as `0`.

For DLGR and DHGR the signal is read from the main bank only. The corresponding auxiliary-bank hole bytes carry no meaning and SHOULD be zero (auxiliary text-page holes are used by the 80-column firmware and MUST NOT be relied upon).

### 3.3 Arming and disarming

A producer MUST fill both pages completely before writing mode `1` or `2`. To disarm, write `0` to the mode byte; the signature bytes MAY remain in place. Because the signal lives inside page 2, loading a new single-page image over the pair naturally overwrites it.

The signal is level-sampled: a consumer SHOULD evaluate it once per output frame, not edge-detect writes. Arming and disarming therefore take effect at the next frame boundary.

### 3.4 Mode gating

The signal applies to graphics modes only. A consumer MUST honor it only while the `TEXT` soft switch is off, and MUST read it from the location matching the mode family the soft switches currently select: the hires location while `HIRES` is on, the lores location otherwise. This covers LGR, DLGR, HGR, and DHGR, mixed or full-screen. In text modes the signal is ignored — not cleared — so a program that drops to text (a BASIC prompt, for example) automatically suspends paged presentation and resumes it when graphics return.

In mixed mode the bottom four text rows follow the machine's normal text-page selection per field; a producer using mixed mode SHOULD keep both text pages identical, or avoid mixed mode. (For the lores family the "text pages" are the image pages themselves — mixed-mode lores pairs are inherently self-consistent.)

## 4. Mode 1: interlace

Interlace is spatial. The two pages are the even and odd fields of one double-height image. For a mode with `R` rows per page:

```text
output row 2*y     = page 1, source row y
output row 2*y + 1 = page 2, source row y
```

with `0 <= y < R`, producing `2*R` distinct output rows and no line doubling:

| Mode | Single-page image | Interlaced image |
| --- | --- | --- |
| LGR | 40 × 48 | 40 × 96 |
| DLGR | 80 × 48 | 80 × 96 |
| HGR | 280 × 192 | 280 × 384 |
| DHGR | 560 × 192 | 560 × 384 |

Consumers MUST take field selection from the output row parity only, ignoring the live `PAGE2` state while interlace is armed; the producer's two pages are both part of the current image, and there is no "displayed page" in this mode.

Each field's rows SHOULD be rendered through the implementation's normal single-page pixel pipeline (color/artifact model, monochrome modes, and so on) so that an interlaced image looks like two ordinary frames woven together.

## 5. Mode 2: page flip

Page flip is temporal. Each page is a complete, self-sufficient frame:

```text
even display frame = page 1
 odd display frame = page 2
```

The intent is 120 Hz or faster presentation, where alternating pages every output frame blends optically. On outputs slower than 120 Hz, alternating visibly strobes; a consumer running below 120 Hz MUST NOT alternate and instead MUST present the 50/50 blend of both pages at every frame:

```text
output_pixel = (render(page1) + render(page2)) / 2
```

The blend operates on rendered pixels — after each page has gone through the normal pixel pipeline — not on raw memory bytes, whose artifact colors do not combine linearly.

Consumers that alternate MUST select the page from a single frame-parity value for all data the frame touches, mirroring the coherence rule of the SHR page-flip mode.

## 6. SDD reference behavior

SuperDuperDisplay samples the `A2Li` signal from its main-memory shadow at every frame start. The `[ LEGACY PAGING ]` controls default to **Automatic (A2Li)** and retain Force Interlace / Force Page Flip options for testing files that lack an in-band marker. The sampled or forced value feeds the legacy beam shader's `pagingMode` uniform (`A2VideoManager.cpp`, `A2WindowBeam.cpp`, `shaders/a2video_beam_legacy.frag`).

Implementation notes, for compatibility work:

- While paging is active the beam capture stores a second copy of every visible byte in the upper half of legacy VRAM. The primary half holds page 1 and the secondary half holds page 2, independent of live `PAGE2`. Automatic `A2Li` paging is graphics-gated as specified in section 3.4; a forced override can still page-double text for diagnostics.
- The fragment shader shifts odd output lines into the secondary VRAM half for interlace, and shifts entire odd-parity frames there for page flip.
- Page-flip parity advances on every host render, even when the captured Apple II frame has not changed, so 120 Hz and faster outputs alternate coherent whole pages at output cadence.
- Below 120 Hz, page flip automatically enables the post-processor's rendered-frame pair merge. It performs the 50/50 blend after legacy rendering (and after the optional NTSC pass), skips the unmerged member of each pair, and presents only blended frames. The manual `Merge Frame Pairs` checkbox remains available independently.

## 7. Appletini ONE reference behavior

The Appletini ONE renderer honors the `A2Li` signal, sampling it from its main-memory shadow at every frame start (`legacy_paged_mode()` in `apple_cycle_renderer.c` of the appletini-one repository):

- Interlace synthesizes the double-height frame shadow-side, rendering even rows with `PAGE2` forced off and odd rows with `PAGE2` forced on (`80STORE` forced clear so `PAGE2` swaps pages), each row through the normal mode steppers with the standard per-scanline chroma reset. The IIgs border is unavailable while interlaced (the doubled rows exceed the bordered frame layout).
- Page flip renders both pages fully every frame and blends them with an exact per-channel average — its HDMI output runs below 120 Hz, so the section 5 merge rule always applies.
- All color modes, monochrome included, pass through their usual pixel pipelines in both paged modes.

## 8. File conventions

These container layouts are conventions of the Appletini demo tooling for distributing legacy images; the live-memory protocol above is authoritative. All are raw page dumps with no header, holes included. Double-resolution files store the auxiliary bank first, then the main bank, for each page.

| Extension | Size | Contents |
| --- | ---: | --- |
| `.lgr` | 1,024 | One LGR page |
| `.lgri` | 2,048 | LGR page 1, then page 2 |
| `.dlr` | 2,048 | One DLGR page: aux 1 KB, then main 1 KB |
| `.dlri` | 4,096 | Two `.dlr` pairs: page-1 pair, then page-2 pair |
| `.hgr` | 8,192 | One HGR page |
| `.hgri` | 16,384 | HGR page 1, then page 2 |
| `.dhr` | 16,384 | One DHGR page: aux 8 KB, then main 8 KB |
| `.dhri` | 32,768 | Two `.dhr` pairs: page-1 pair, then page-2 pair |

Because the signal lives inside page 2, two-page files are self-describing: a producer SHOULD store the `A2Li` signature and the intended mode byte in the page-2 hole bytes of the file itself (main bank for `.dlri`/`.dhri`), so that staging both pages into memory arms the correct mode with no side channel. The same two-page container then distributes interlace and page-flip content distinguishably — the mode byte, not the extension, declares which.

A loader encountering a two-page file without the in-file signature MAY arm a mode chosen by other means (user selection, catalog metadata) by writing the signal itself after staging.

## 9. Minimal producer recipe

1. Load or draw field/frame A into page 1 and field/frame B into page 2 (both banks for DLGR/DHGR).
2. Select the graphics mode normally (`TEXT` off; `HIRES` for the hires family; plus `80COL`/`AN3` for double modes) with page 1 displayed.
3. If the staged page 2 did not already carry the signal, write it — HGR/DHGR shown; use `$0878-$087C` for the lores family:

```asm
    lda #$C1        ; 'A' | $80
    sta $4078
    lda #$B2        ; '2' | $80
    sta $4079
    lda #$CC        ; 'L' | $80
    sta $407A
    lda #$E9        ; 'i' | $80
    sta $407B
    lda #$01        ; 1 = interlace, 2 = page flip
    sta $407C
```

4. To return to single-page display, `lda #$00 : sta $407C` (or `$087C`) before reusing either page.

## 10. Conformance checklist

- Signature is compared as four bytes in address order at page 2 offset `+$78`; all four are required.
- The signal is read from the main bank; auxiliary hole bytes carry no meaning.
- The lores-family signal is at `$0878`; the hires-family signal is at `$4078`; the family follows the live soft switches.
- Mode byte values other than `1` and `2` disarm; values `3-255` are reserved.
- The signal is honored only while `TEXT` is off.
- The signal is level-sampled per frame; it is not an edge-triggered command.
- Interlace: even output rows from page 1, odd from page 2, `2*R` distinct rows, no line doubling, live `PAGE2` ignored.
- Page flip at 120 Hz or above: one whole page per frame, page chosen by a single parity value.
- Page flip below 120 Hz: both pages rendered and blended 50/50 every frame; never alternated.
- Blending operates on rendered pixels, not on raw page bytes.
- Two-page files are aux-first per page and SHOULD carry the signature and mode in their page-2 hole bytes.
