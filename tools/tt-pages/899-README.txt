Teletext decoder test page 899
==============================

Files
-----
899.tti
    Four-subpage MRG TTI carousel for VBIT/VBIT2-compatible tools.
    Teletext control codes use the standard 8-bit TTI translation (80h..9Fh).

teletext_test_899_raw.json
    Simple JSON test-data format. Every row is exactly 40 raw Teletext bytes,
    represented as hexadecimal. This is intentionally NOT claimed to be the
    current NOS JSON schema; it is meant to be deterministic decoder input.

teletext_test_899.h
    C header containing the same 4 x 24 x 40 raw-byte pages.

teletext_test_899_annotated.txt
    Human-readable dump. Control bytes are shown as <HH>.

Subpages
--------
899/1  Complete G0 alphanumeric code positions 20h..7Fh.
899/2  Complete G1 mosaic code positions 20h..7Fh, contiguous and separated,
       plus hold/release graphics.
899/3  All foreground colours, all backgrounds, flash/steady, conceal.
899/4  Normal, double-height, double-width, double-size and combinations.

Notes
-----
The TTI page status selects the German national-option set, traditionally also
used for Dutch/Flemish decoding. The raw JSON and C header contain the literal
code positions, so the decoder itself determines the displayed national glyphs.

Double-height rows deliberately leave the following physical row blank because
the decoder should display the lower half there.

Page 899 is used because Teletext magazines are 1..8; page 999 would not be a
valid conventional World System Teletext page number.
