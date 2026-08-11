# SHR-3200 (Brooks) and SHR4 Video Modes

Implementation specification, revision 1.1 (2026-08-10)

## 1. Purpose and scope

This document specifies the memory-level interface and reference decoding behavior for:

- SHR-3200, also called Brooks-3200;
- SHR4 standard SHR (`mode 0`);
- SHR4 RGGB (`mode 1`);
- SHR4 PAL256 (`mode 2`);
- SHR4 R4G4B4 (`mode 3`); and
- the two-page interlace and page-flip extensions shared by SHR4 and SHR-3200.

The intended audience is authors of Apple IIgs software, video capture devices, emulators, display cards, converters, and standalone image viewers. The format extends the Apple IIgs Super Hi-Resolution (SHR) memory layout; an implementation therefore needs a normal SHR decoder as its foundation.

The normative words **MUST**, **MUST NOT**, **SHOULD**, and **MAY** describe interoperability requirements. Sections explicitly labeled "SDD reference behavior" describe SuperDuperDisplay's current implementation, including behavior that another implementation need not reproduce unless pixel-for-pixel compatibility is required.

Primary reference sources in this repository are:

- `shaders/a2video_beam_shr_raw.frag` — pixel decoding and RGGB reconstruction;
- `A2VideoManager.cpp` — beam-time capture, recognition, color fill, line palettes, and PAL256 snapshots;
- `common.h` — addresses and magic values;
- `A2WindowBeam.cpp` — page selection and GPU buffer layout; and
- `extras/MemoryLoader.cpp` — standalone `.shr` file loading conventions.

## 2. Base SHR memory layout

The primary SHR page is in bank `$E1` (auxiliary memory). A second page, when requested, is at the same addresses in bank `$E0` (main memory).

All addresses in this document are bank-relative 16-bit addresses unless a bank is shown explicitly.

| Address range | Size | Meaning |
| --- | ---: | --- |
| `$2000-$9CFF` | `$7D00` (32,000) bytes | 200 image rows, 160 bytes per row |
| `$9D00-$9DC7` | 200 bytes | One Scanline Control Byte (SCB) per row |
| `$9DC8-$9DF7` | 48 bytes | Reserved by base SHR |
| `$9DF8-$9DFB` | 4 bytes | Extension control block |
| `$9DFC-$9DFF` | 4 bytes | Extension magic |
| `$9E00-$9FFF` | `$0200` (512) bytes | 16 palettes × 16 colors × 2 bytes |

The complete conventional SHR page from `$2000` through `$9FFF` is therefore `$8000` bytes.

### 2.1 Image row addressing

For source row `y`, where `0 <= y < 200`, and byte column `xb`, where `0 <= xb < 160`:

```text
image_byte(y, xb) = memory[$2000 + 160*y + xb]
```

### 2.2 Scanline Control Byte

For source row `y`:

```text
scb(y) = memory[$9D00 + y]
```

The base SHR meanings remain in force:

| Bit(s) | Meaning |
| --- | --- |
| `0-3` | Normal SHR palette number (`0-15`) |
| `4` | Reserved; producers MUST write zero |
| `5` | 320-mode color fill enable |
| `6` | Scanline interrupt request |
| `7` | `0`: 320-sample mode; `1`: 640-sample mode |

SuperDuperDisplay uses SCB bit 4 only inside its private captured-line buffer to mark an unused line. That private use is not part of the memory format.

RGGB, PAL256, and R4G4B4 producers SHOULD clear bit 5 because zero-valued nibbles are image data in those encodings. If bit 5 is set, an SDD-compatible capture path applies ordinary SHR color fill before decoding the extension.

### 2.3 IIgs color word and byte order

A normal IIgs color is a 12-bit `$0RGB` value:

```text
bits 11..8 = red
bits  7..4 = green
bits  3..0 = blue
```

It is stored little-endian:

```text
address + 0: GGGGBBBB
address + 1: 0000RRRR
```

SHR4 assigns a meaning to the otherwise unused high nibble of the second byte:

```text
address + 1: MMMMRRRR
             ^^^^
             SHR4 mode selector
```

The 12 color bits remain in their normal locations.

### 2.4 Normal 320-mode pixel extraction

Each image byte contains two 4-bit samples, high nibble first:

```text
sample(y, x) = image_byte(y, floor(x/2)) >> 4     if x is even
             = image_byte(y, floor(x/2)) & $0F   if x is odd
```

Here `0 <= x < 320`. A 640-wide renderer displays each sample twice horizontally. A non-interlaced 400-line renderer displays each source row twice vertically.

If color fill is enabled, process samples from left to right using the normal SHR rule: a zero sample repeats the preceding nonzero sample, with zero remaining zero at the left edge.

### 2.5 Normal 640-mode extraction and palette selection

Each image byte contains four 2-bit samples, most significant pair first. Let `q = x mod 4`, `raw` be the selected 2-bit value, and `x` range from 0 through 639:

```text
raw = (image_byte(y, floor(x/4)) >> (6 - 2*q)) & 3
```

The palette entry is selected as follows:

| `q` | Palette entry for `raw = 0, 1, 2, 3` |
| ---: | --- |
| 0 | `8, 9, 10, 11` |
| 1 | `12, 13, 14, 15` |
| 2 | `0, 1, 2, 3` |
| 3 | `4, 5, 6, 7` |

This table is important for SHR4: the selected entry supplies both the mode selector and, for mode 0, the displayed color.

## 3. Extension recognition and control block

### 3.1 Magic bytes

The four bytes at `$9DFC-$9DFF` are compared byte-for-byte. They are high-bit-set ASCII:

| Format | Bytes at `$9DFC-$9DFF` | Text with bit 7 cleared |
| --- | --- | --- |
| SHR4 | `$D3 $C8 $D2 $B4` | `SHR4` |
| SHR-3200 | `$B3 $B2 $B0 $B0` | `3200` |

Implementations MUST compare the byte sequence, not a host-endian 32-bit integer. On a little-endian host the same four bytes appear as integers `$B4D2C8D3` and `$B0B0B2B3`, respectively; on a big-endian host they appear as `$D3C8D2B4` and `$B3B2B0B0`.

SHR4 and SHR-3200 are mutually exclusive for a page because the same four bytes identify them.

### 3.2 Control bytes

The control block is stored in address order:

| Address | Name | Values |
| --- | --- | --- |
| `$9DF8` | `page_mode` | `0`: single page; `1`: interlace; `2`: page flip |
| `$9DF9` | `palette_bank` | SHR-3200: `0` = `$E0`, `1` = `$E1` |
| `$9DFA` | `palette_address_lo` | SHR-3200 line-palette pointer, low byte |
| `$9DFB` | `palette_address_hi` | SHR-3200 line-palette pointer, high byte |

For SHR4, only `page_mode` is defined; the remaining three control bytes SHOULD be zero.

For SHR-3200:

```text
palette_address = palette_address_lo + 256*palette_address_hi
```

The line-palette table is 6,400 bytes long and MUST be contiguous within the selected bank. It MUST NOT wrap at `$FFFF`. It SHOULD reside entirely below `$C000` so that it does not overlap the Apple IIgs I/O and firmware address space.

SDD reference behavior: only a `palette_bank` value of `1` selects `$E1`; every other value selects `$E0`. Its current shadow-memory safety check requires `palette_address < $A700`.

Unknown `page_mode` values are reserved. A robust consumer SHOULD treat them as `0`.

## 4. SHR4 common decoding rules

SHR4 is active only when the SHR4 magic is present. The normal SCB selects one of the 16 conventional palettes for the row. A pixel is first decoded to its normal SHR palette entry as described in sections 2.4 and 2.5. The high nibble `M` of that selected entry's second byte chooses the decoder:

| `M` | Name | Meaning |
| ---: | --- | --- |
| 0 | SHR | Standard palette color |
| 1 | RGGB | Raw Bayer/CFA sample reconstructed to RGB |
| 2 | PAL256 | Field-wide packed image; each image byte indexes one 256-entry palette |
| 3 | R4G4B4 | Nibble stream contains direct 4:4:4 RGB components |
| 4-15 | Reserved | Invalid/undefined |

Mode selection is **per selected palette entry** for standard SHR, RGGB, and R4G4B4. Those modes can coexist on the same row. PAL256 is the exception: because it changes the image area's geometry, the presence of selector `2` declares a field-wide PAL256 image and takes precedence over per-entry decoding. A PAL256 producer MUST put selector `2` in every palette entry so recognition is unambiguous and every byte value remains a valid index.

The mode selector belongs to the center/output sample. RGGB neighbor samples contribute their raw numeric values even if those neighbors would select another mode when rendered at their own positions.

A consumer encountering selector `4-15` SHOULD fall back to standard SHR using the low 12 color bits or display a diagnostic color. Current SDD shader output is undefined for these selectors.

## 5. SHR4 mode 0: standard SHR

Mode 0 behaves exactly like normal SHR:

1. Use the SCB palette number to select one 16-entry palette.
2. Decode the image byte as 320 or 640 mode.
3. Select a palette entry.
4. Interpret its low 12 bits as `$0RGB`.

Mode 0 permits an SHR4 page to retain ordinary SHR colors alongside any of the three extended encodings.

## 6. SHR4 mode 1: RGGB

### 6.1 Stored samples and CFA arrangement

RGGB treats each decoded raw sample as intensity rather than as a palette color:

- in 320 mode, samples are 4-bit values `0-15`, producing a 320-sample-wide CFA;
- in 640 mode, samples are 2-bit values `0-3`, producing a 640-sample-wide CFA.

The palette's RGB bits do not contribute to the reconstructed color. Producers conventionally store `$1ggg` in a marker entry, where `g` equals the entry's raw intensity, so a non-SHR4 display can show a useful grayscale approximation. The decoder only requires the selector nibble to be `1`.

The CFA repeats as:

```text
even CFA row: R G R G R G ...
 odd CFA row: G B G B G B ...
```

Thus the 2×2 cell is:

```text
R G
G B
```

For a single-page image, source rows `0-199` determine CFA row parity and are normally doubled only after reconstruction for a 400-line presentation. For interlace, the two banks form 400 distinct CFA rows as specified in section 10.

### 6.2 Required neighborhood

For each center sample `O`, gather this 13-sample subset of a 5×5 neighborhood:

```text
            s0
        s1  s2  s3
    s4  s5  s6  s7  s8
        s9 s10 s11
           s12
```

`s6` is the center. In `(dx, dy)` form:

```text
s0=( 0,-2)
s1=(-1,-1) s2=(0,-1) s3=(1,-1)
s4=(-2, 0) s5=(-1,0) s6=(0,0) s7=(1,0) s8=(2,0)
s9=(-1, 1) s10=(0,1) s11=(1,1)
s12=(0, 2)
```

Consumers SHOULD use zero for samples outside the 320/640 by 200/400 image. SDD explicitly returns zero for out-of-range horizontal fetches and masks every vertical tap that falls outside the image.

### 6.3 Reference linear filters

Define these weighted sums over `s0..s12`:

```text
G =    -1*s0 +0*s1 +2*s2 +0*s3
      -1*s4 +2*s5 +4*s6 +2*s7 -1*s8
       0*s9 +2*s10+0*s11-1*s12

XG =  0.5*s0 -1*s1 +0*s2 -1*s3
      -1*s4  +4*s5 +5*s6 +4*s7 -1*s8
      -1*s9  +0*s10-1*s11+0.5*s12

XGX = -1*s0 -1*s1 +4*s2 -1*s3
       0.5*s4+0*s5 +5*s6 +0*s7 +0.5*s8
      -1*s9 +4*s10-1*s11-1*s12

RB = -1.5*s0+2*s1 +0*s2 +2*s3
     -1.5*s4+0*s5 +6*s6 +0*s7 -1.5*s8
       2*s9 +0*s10+2*s11-1.5*s12
```

Choose components from CFA position:

| Center position | Red numerator | Green numerator | Blue numerator |
| --- | ---: | ---: | ---: |
| R: even x, even y | `8*s6` | `G` | `RB` |
| G in R row: odd x, even y | `XG` | `8*s6` | `XGX` |
| G in B row: even x, odd y | `XGX` | `8*s6` | `XG` |
| B: odd x, odd y | `RB` | `G` | `8*s6` |

Normalize each component by:

```text
320 mode: 15 * 8 = 120
640 mode:  3 * 8 = 24
```

Clamp each result to `[0,1]` and set alpha to 1. These filters are the exact SDD reference behavior and are based on a Malvar-He-Cutler-style linear demosaic.

An implementation MAY use a different demosaic algorithm, but will not be pixel-identical to SDD. It MUST preserve the raw sample order and RGGB phase.

## 7. SHR4 mode 2: PAL256

### 7.1 Packed pixel encoding

PAL256 treats each complete image byte as one 8-bit palette index. It reinterprets the contiguous `$7D00`-byte image area as 100 rows of 320 bytes instead of 200 rows of 160 bytes:

```text
pal256_index(y, x) = memory[$2000 + 320*y + x]
color(y, x) = palette256[pal256_index(y, x)] & $0FFF

0 <= x < 320
0 <= y < 100
```

Equivalently, each packed PAL256 row joins two consecutive physical SHR image rows side by side:

```text
packed x   0..159 = physical SHR row 2*y,     byte 0..159
packed x 160..319 = physical SHR row 2*y + 1, byte 0..159
```

The 256-entry palette is the normal 512-byte palette region at `$9E00-$9FFF`, treated as one contiguous array. Entry `i` is stored at `$9E00 + 2*i` in normal little-endian IIgs color order. The SHR4 selector occupies the unused high nibble and does not alter the low 12-bit color.

The native PAL256 raster is 320×100. Each PAL256 pixel spans two samples in SDD's 640-wide output. A progressive presentation repeats every PAL256 row four times to fill 640×400.

PAL256 is field-wide and cannot be spatially mixed with standard SHR, RGGB, or R4G4B4. Producers MUST set selector `2` on all 256 palette entries.

SCBs do not change PAL256's packed byte geometry. Producers SHOULD clear SCB bits 7 and 5 for compatibility with recognizers that inspect the conventional SHR control plane.

### 7.2 PAL256i (`page_mode = 1`)

PAL256i is one 320×200 image made by vertically concatenating two complete 320×100 PAL256 fields:

```text
PAL256i rows   0.. 99 = auxiliary bank $E1 packed rows 0..99
PAL256i rows 100..199 = main bank      $E0 packed rows 0..99
```

The first field therefore supplies the top half and the second field supplies the bottom half. PAL256i MUST NOT alternate or ping-pong between auxiliary and main memory by output-row parity. A 640×400 presentation doubles each of the resulting 200 rows vertically.

Both banks MUST contain the SHR4 magic, `page_mode = 1`, a complete packed image field, and a valid 256-color palette. Each half is decoded against the palette state from its own bank.

`page_mode = 2` remains PAL256 page flip: it selects one complete 320×100 field per displayed frame (or blends the two complete fields under the sub-120 Hz merge rule). It is distinct from PAL256i.

### 7.3 Beam-racing requirement

The IIgs palette can change while a physical SHR row is being scanned. Correct live-video decoding MUST use the color stored in palette entry `index` at the instant the corresponding image byte is fetched by the video beam. Looking up all bytes against the palette state at end-of-frame is not equivalent.

An SDD-compatible capture implementation snapshots one 12-bit color per physical image byte, then reinterprets the contiguous snapshot buffer using the packed coordinates from section 7.1:

```text
snapshot[source_y][source_x] =
    palette_state_at_beam_time[image_byte(source_y, source_x)]

pal256_snapshot[packed_y][packed_x] =
    snapshot[2*packed_y + floor(packed_x/160)][packed_x mod 160]
```

This is a 320×100 array of 16-bit words for one bank. PAL256i and page flip store a second 320×100 array immediately after the first. A static image decoder, for which palette memory does not change during display, can perform the lookup directly.

## 8. SHR4 mode 3: R4G4B4

### 8.1 Pixel encoding

R4G4B4 ignores palettes after using their high nibble to select the mode. It interprets the row as a stream of 4-bit components. For three consecutive bytes:

```text
bytes:       AB CD EF
nibbles:      A B C D E F
RGB pixels:  (A,B,C) (D,E,F)
```

Each RGB pixel spans three logical 320-mode sample positions, which are six samples in a 640-wide output. For example:

```text
bytes $C3 $D4 $E5 -> color $C3D for 6 output samples,
                     color $4E5 for the next 6 output samples
```

For logical 320-coordinate `x`, let `p = x mod 6` within a three-byte group:

```text
p = 0,1,2 -> RGB = (A,B,C)
p = 3,4,5 -> RGB = (D,E,F)
```

The nominal horizontal resolution is `320/3`, or approximately 106.67 RGB pixels per row. There are 53 complete three-byte groups (106 complete RGB pixels) in the first 159 bytes. The final byte cannot form a complete RGB triplet. Producers SHOULD set the final byte to zero and consumers SHOULD display the incomplete right-edge pixel as black or crop it. SDD currently reads the following staging texel, which is right-border data when a right border exists and an out-of-bounds texture access when it does not. The final partial pixel is therefore not portable.

R4G4B4 is always a 320-mode nibble-stream encoding regardless of SCB bit 7. Producers MUST clear SCB bits 7 and 5 and SHOULD put selector `3` in all 16 entries of the SCB-selected palette. The low 12 bits of those marker entries are ignored by the R4G4B4 decoder.

## 9. SHR-3200 (Brooks-3200)

### 9.1 Overview

SHR-3200 uses an independent 16-color palette for every one of the 200 source rows. It can therefore use as many as 3,200 palette colors across one image, although only 16 entries are available on any one row. It is not an SHR4 selector mode and MUST be recognized by the `3200` magic.

### 9.2 Line-palette table

The control block points to 200 consecutive 32-byte palette records:

```text
line_palette(y) = selected_bank[palette_address + 32*y]
```

Each record contains 16 little-endian `$0RGB` colors in entry order `0-15`. The decoder uses this record instead of the palette chosen by SCB bits 0-3. SCB bit 7 still determines 320 versus 640 pixel extraction, and SCB bit 5 still requests ordinary 320-mode color fill.

### 9.3 Reversed palette index

Brooks-3200 reverses the decoded palette index before accessing the row palette:

```text
brooks_index = 15 - normal_shr_palette_index
color = line_palette(y)[brooks_index]
```

This reversal applies after both 320-mode nibble extraction and 640-mode palette-index mapping. The conventional Brooks format is 320 mode; producers SHOULD clear SCB bit 7 unless a 640-mode extension is intentional.

The palette records' high mode nibbles have no SHR4 meaning under `3200` magic and SHOULD be zero.

## 10. Two-page modes

`page_mode` applies to SHR4 and SHR-3200. Page A is the SHR page in `$E1`; page B is the same `$2000-$9FFF` layout in `$E0`. PAL256 uses the same two banks but gives interlace mode the special fixed-half mapping in section 7.2.

Both pages SHOULD contain matching extension magic and valid control data. This is especially important to SDD, which captures SCBs, palettes, and mode metadata independently from each bank. For two-page SHR-3200, each page MUST provide a valid pointer to the line-palette table appropriate to that page.

### 10.1 Single page (`page_mode = 0`)

Use only page A (`$E1`). Ordinary SHR4 and SHR-3200 present 200 source rows, normally doubled vertically to 400 display rows. PAL256 presents its packed 320×100 field, with every row repeated four times for a 640×400 presentation.

### 10.2 Interlace (`page_mode = 1`)

For standard SHR4, RGGB, R4G4B4, and SHR-3200, construct a 400-row image by alternating banks at the same source-row number:

```text
output row 2*y     = page A ($E1), source row y
output row 2*y + 1 = page B ($E0), source row y
```

Do not vertically duplicate either source row. RGGB CFA parity is based on the resulting 400-row output: page A supplies even CFA rows and page B supplies odd CFA rows.

PAL256i is the exception. It constructs a 320×200 image by placing all 100 packed rows from page A above all 100 packed rows from page B, as specified in section 7.2. It does not alternate banks by row.

### 10.3 Page flip (`page_mode = 2`)

Display one entire page per output frame:

```text
even display frame = page A ($E1)
 odd display frame = page B ($E0)
```

Each selected non-PAL256 page remains a 200-row image and may be doubled vertically for presentation. Each selected PAL256 page is one packed 320×100 field and is quadrupled vertically. The page selector MUST be coherent for image bytes, SCBs, normal palettes, SHR-3200 line palettes, and PAL256 palette snapshots.

> NOTE: SDD selects page-flip data for image bytes, palettes, and PAL256 snapshots from one rendered-frame parity value; implementations SHOULD likewise use a single parity value everywhere. The original idea for page flip was to take advantage of high frame rate monitors to blend fast flipping images. Below 120 Hz, SDD automatically blends each pair of page-flip frames and halves the presented frame rate. The CRT shader (F3) checkbox `Merge Frame Pairs` remains available to force the same behavior independently.

## 11. Beam-time capture order

A live capture device that aims to reproduce SDD behavior SHOULD operate in this order for each source row:

1. At the start of the row, snapshot the SCB.
2. Snapshot the SCB-selected 16-color palette.
3. Recognize the extension magic and read its control block.
4. For SHR-3200, replace the selected palette snapshot with the row's 32-byte line-palette record.
5. As each group of image bytes is fetched by the beam, snapshot those bytes.
6. If 320-mode color fill is enabled, apply fill from left to right to the captured nibbles.
7. If PAL256 is present, snapshot the full-palette color indexed by every captured image byte at that same beam time, then reinterpret each bank's contiguous snapshots as 320×100.
8. Repeat independently for page B when PAL256i or another two-page mode is active.

The ordinary selected palette is stable for a row because SHR hardware latches it at row start. PAL256 is the exception: it deliberately observes the live 256-entry palette throughout the row.

## 12. Standalone `.shr` file conventions

The live-memory protocol is authoritative. The following container layouts describe files accepted by SDD's memory loader.

Addresses `$2000-$9FFF` map to file offsets `$0000-$7FFF`. Consequently:

```text
control block file offset = $9DF8 - $2000 = $7DF8
magic file offset         = $9DFC - $2000 = $7DFC
palette file offset       = $9E00 - $2000 = $7E00
```

### 12.1 Single-page files

| Size | Contents |
| ---: | --- |
| `$8000` | One complete standard SHR or SHR4 page |
| `$9900` | One `$8000` SHR-3200 page followed by `$1900` bytes of line palettes |

For the `$9900` convention, the pointer in the page's control block tells the loader where to place the appended palette data in emulated memory. The appended bytes are ordered as row 0's 32-byte record through row 199's record.

The bundled `samples/SHR 3200/beach.shr` is a `$9900` example. Its metadata bytes at file offset `$7DF8` are:

```text
00 00 00 20 B3 B2 B0 B0
|  |  -----  -----------
|  |  $2000  high-bit "3200"
|  bank $E0
single page
```

### 12.2 Multi-page files recognized by SDD

SDD recognizes these concatenations when the first page is SHR-3200:

| Total size | Contents |
| ---: | --- |
| `$11900` | SHR-3200 page + its palettes + one `$8000` standard/SHR4 page |
| `$13200` | SHR-3200 page + palettes + a second SHR-3200 page + palettes |
| `$11600` | SHR-3200 page + palettes + `$7D00` raw image bytes for page B; page A metadata is copied for the remainder |

It also recognizes a `$10000` file as two complete `$8000` SHR/SHR4 pages. These are SDD loader conventions rather than extra bytes in the live-memory protocol.

## 13. Minimal producer recipes

### 13.1 Uniform SHR4 image

1. Build an ordinary `$8000` SHR page in `$E1:$2000-$9FFF`.
2. Write `$D3 $C8 $D2 $B4` at `$9DFC`.
3. Write the requested `page_mode` at `$9DF8` and clear `$9DF9-$9DFB`.
4. Clear SCB color fill for all extended raw-data modes.
5. Put selector `0`, `1`, or `3` into the high nibble of every palette entry that image samples can select. For PAL256, put selector `2` into all 256 entries because PAL256 is field-wide.
6. For RGGB, write raw intensities in normal 320/640 sample order.
7. For PAL256, write 100 packed rows of 320 byte-indices contiguously from `$2000` and populate all 256 low-12-bit palette colors.
8. For R4G4B4, write RGB component nibbles in `R,G,B,R,G,B,...` order and use 320-mode SCBs.

### 13.2 SHR-3200 image

1. Build the normal image bytes and SCBs in `$E1:$2000-$9DC7`.
2. Allocate 6,400 contiguous bytes for the 200 line palettes.
3. Write the palette bank and little-endian address into `$9DF9-$9DFB`.
4. Write `$B3 $B2 $B0 $B0` at `$9DFC`.
5. To display line-palette entry `j`, encode the normal SHR sample that selects entry `15-j` because Brooks lookup reverses the index.
6. For a standalone file, append the 6,400 palette bytes after the complete `$8000` page.

### 13.3 Two-page image

1. Put page A at `$E1:$2000-$9FFF` and page B at `$E0:$2000-$9FFF`.
2. Write the same nonzero `page_mode` and appropriate magic in both pages.
3. Give each SHR-3200 page its own valid palette-table pointer.
4. Keep palette selectors and SCB modes valid independently on both pages.
5. For PAL256i, put the top 100 packed rows in page A and the bottom 100 packed rows in page B; do not split the image into alternating rows.

## 14. Decoder outline

The following pseudocode intentionally separates mode selection from mode rendering:

```text
decode_frame(memory, frame_parity):
    page_mode = sanitize(memory[E1:$9DF8])

    if magic(E1) == "SHR4" and field_uses_selector(E1, 2):
        for output_y in 0..399:
            if page_mode == INTERLACE:
                combined_y = output_y/2
                bank = E1 if combined_y < 100 else E0
                packed_y = combined_y mod 100
            else:
                bank = E0 if page_mode == PAGE_FLIP and frame_parity == 1
                          else E1
                packed_y = output_y/4

            for output_x in 0..639:
                packed_x = output_x/2
                output pal256_snapshot[bank][packed_y][packed_x]
        return

    for output_y in presentation_height:
        (bank, source_y) = select_page_and_row(page_mode,
                                                output_y,
                                                frame_parity)
        scb = bank[$9D00 + source_y]
        row_bytes = bank[$2000 + 160*source_y .. +159]

        if magic(bank) == "3200":
            palette = line_palette_record(bank, source_y)
            reverse_index = true
            shr4 = false
        else:
            palette = scb_selected_palette(bank, scb)
            reverse_index = false
            shr4 = (magic(bank) == "SHR4")

        optionally_apply_320_fill(row_bytes, scb)

        for output_x in 0..639:
            normal_index = decode_normal_shr_index(row_bytes, scb, output_x)
            index = 15-normal_index if reverse_index else normal_index

            if not shr4:
                output palette[index]
                continue

            selector = high_nibble(palette[index].high_byte)
            switch selector:
                0: output palette[index] & $0FFF
                1: output demosaic(raw_sample_neighborhood,
                                   output_x, output_y)
                2: invalid mixed PAL256 declaration; use deterministic fallback
                3: output r4g4b4_triplet(row_bytes, output_x/2)
                default: output implementation-defined fallback
```

An implementation without beam-racing input can replace every snapshot with a direct memory lookup.

## 15. Conformance test vectors

These small vectors exercise the byte ordering and index rules without requiring a complete image.

### 15.1 Recognition

```text
bytes at $9DFC: D3 C8 D2 B4 -> SHR4
bytes at $9DFC: B3 B2 B0 B0 -> SHR-3200
control bytes:  01 01 34 12 -> interlace, palettes in $E1 at $1234
```

### 15.2 320-mode sample order

```text
image byte $A5 -> logical samples $A, $5
640-wide presentation -> $A, $A, $5, $5
Brooks indices -> 5, 10
```

### 15.3 640-mode mapping

```text
image byte $1B = samples 00, 01, 10, 11
palette entries at q=0,1,2,3 -> 8, 13, 2, 7
```

### 15.4 PAL256

```text
image byte $A5 -> palette256 entry 165
the corresponding PAL256 pixel uses entry 165
640-wide output samples 2*x and 2*x+1 both use that color

$2000 -> packed coordinate (0,0)
$209F -> packed coordinate (159,0)
$20A0 -> packed coordinate (160,0)
$213F -> packed coordinate (319,0)
$2140 -> packed coordinate (0,1)
```

The byte MUST NOT be interpreted as separate palette entries `$A` and `$5` after PAL256 dispatch.

For PAL256i, combined row 99 comes from AUX packed row 99 and combined row 100 comes from main packed row 0. No combined row alternates banks with its neighbor.

### 15.5 R4G4B4

```text
input bytes: $C3 $D4 $E5
first RGB pixel:  R=$C G=$3 B=$D
second RGB pixel: R=$4 G=$E B=$5
```

Each result occupies six samples in a 640-wide presentation.

### 15.6 RGGB phase

For the raw 2×2 block below, a no-filter diagnostic decoder must identify the sites exactly as shown:

```text
source row 0: R=1 G=2
source row 1: G=3 B=4
```

The reference demosaic uses those source-site values as the known component at each position and reconstructs the other two components with section 6.3's filters.

## 16. Conformance checklist

An interoperable implementation should verify all of the following:

- Magic is compared as bytes, independent of host endianness.
- Control pointer bytes are low byte then high byte.
- The primary page is `$E1`; the secondary page is `$E0`.
- A Brooks row palette has exactly 16 entries and palette indices are reversed.
- SHR4 dispatch uses the palette entry selected by the current normal SHR sample.
- SHR4 selectors `0`, `1`, and `3` may coexist on one row; selector `2` declares field-wide PAL256.
- 320 data is high nibble first; 640 data is most significant 2-bit pair first.
- The 640-mode palette mapping matches the table in section 2.5.
- RGGB phase begins with red at `(0,0)` and uses raw sample values, not palette RGB.
- PAL256 packs the `$7D00` image bytes as 320×100, combines each whole byte into one index, and uses a beam-time palette snapshot.
- R4G4B4 groups six consecutive nibbles into two RGB pixels.
- Interlace alternates `$E1/$E0` by output row for non-PAL256 modes; PAL256i concatenates AUX above main; page flip alternates by frame.
- Page selection is identical across image data and all palette sources.
- Unknown selector and page-mode values are handled deterministically.

## 17. Exact SDD color-conversion notes

The mode data stores 4-bit components; their transfer to a display's color space is a renderer policy. For exact current SDD output:

- ordinary SHR, SHR-3200, PAL256, and R4G4B4 map a component `c` to floating point `c/16`, not `c/15`;
- RGGB normalization uses `c/15` in 320 mode or `c/3` in 640 mode through the filter divisors;
- reconstructed RGGB components are clamped after filtering; and
- an optional monochrome stage computes luminance as `0.299R + 0.587G + 0.114B`.

Implementations that map 4-bit color to full-range 8-bit values with `c*17` remain data-format compatible, but will not be pixel-identical to SDD.
