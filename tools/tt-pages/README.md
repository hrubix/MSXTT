# Local Teletekst pages for tt-proxy

JSON fixtures with `"format": "teletext-level1-raw-rows"` are loaded by
`tools/tt-proxy.py` and served as NOS-style `/json/NNN` (+ subpages) instead
of fetching NOS.

## Page 899 (decoder test)

From `teletext_test_899` — four subpages:

| URL | Content |
| --- | --- |
| `/json/899` | G0 alphanumeric 20h..7Fh |
| `/json/899-2` | G1 mosaics + hold/release |
| `/json/899-3` | Colours / flash / conceal |
| `/json/899-4` | Double height / width / size |

MSXTT requests bare `899` for the first subpage and `899-2` … for later ones
(same as NOS). Subpage links use `nextSubPage` / `prevSubPage`.

Flash, conceal, separated mosaics, and double-size are only partially visible:
MSXTT’s parser/renderer is NOS Level‑1 HTML, not a full WST decoder.

## Character audit (live NOS)

```
python tools/check-nos-chars.py --out docs/nos-char-audit.txt
```

Crawls `nextPage` / `nextSubPage` from page 100 (1s between requests). Per page:
`OK` or `UNSUPPORTED: …` vs what [`src/tt/parse.c`](../../src/tt/parse.c) accepts.
Optional `--sweep` also probes bare `100`–`899` (many 404s).
