#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make Snapper's site (GitHub Pages, .github/workflows/firmware.yml):

  index.html                  the start page: the logo, Install, Try, Source
  install/index.html          web/snapper_installer.html, self-contained (fm1pkg.js and fm1ota.js inlined, as
                              Felucca's web/make_site.py does for its installer)
  firmware/snapper-X.Y-VER.fwsc  the package (+ LICENSE, LICENSING.md, LICENSES/: it holds JieLi SDK files under
                              Apache-2.0, see LICENSING.md)
  try/                        the browser build (build/sn_emu, web/emu/sn_build.sh), when it was built
  snapper-logo.png

  web/sn_make_site.py build/felucca.fwsc VERSION OUT_DIR
  web/sn_make_site.py --waiting OUT_DIR       the start page before the first release: it points to dev/

The workflow makes dev's test build in OUT_DIR/dev first, then main's release in OUT_DIR, whose start page then
links the test build; with no release yet, the waiting page. Each try/ is that branch's own browser build:
SN_EMU_DIR names it (default build/sn_emu); SN_SITE_TRY=0 leaves try/ out.

VERSION is the build's (the commit); Snapper's own (X.Y) is SN_VERSION in firmware/src/sn_version.h.

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files); its identity
(FM-1_9xx) is what the FM-1 must report after the install.
"""
import json
import os
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the identity, the inlined modules)


def felucca_version():
    m = re.search(r'#define FELUCCA_VERSION "([^"]*)"', (ROOT / "firmware/src/felucca.c").read_text(encoding="utf-8"))
    return m[1] if m else "?"


def sn_version():
    m = re.search(r'#define SN_VERSION "([^"]*)"', (ROOT / "firmware/src/sn_version.h").read_text(encoding="utf-8"))
    return m[1] if m else "?"


BLURB = """Snappy generative 80s drum-heavy pop for the M-VAVE FM-1, caught in 1985: a drone and a drum machine that
build and build, then the big gated drums breaking in on the downbeat, a drama and a key change. Ten styles; press play."""


def page(body, foot):
    """a start page in Snapper's GATE colours: the logo, the blurb, body (buttons, notes), the footer"""
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Snapper</title><link rel="icon" href="snapper-logo.png">
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin: 0; background: #0e0608; color: #fff1ec; font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI",
         Arial, sans-serif; }}
  main {{ max-width: 540px; margin: 0 auto; padding: 40px 16px; text-align: center; }}
  img {{ width: 220px; height: 220px; border-radius: 10px; }}
  h1 {{ font-size: 28px; margin: 12px 0 4px; }}
  p {{ color: #c8aaa6; margin: 0 0 24px; }}
  p.t {{ color: #ff3b2f; }}
  a {{ color: #5ab8ff; }}
  .b {{ display: inline-block; margin: 6px; padding: 10px 22px; border-radius: 6px; background: #5ab8ff; color: #0e0608;
        font-weight: 600; text-decoration: none; }}
  small {{ display: block; margin-top: 28px; color: #c8aaa6; }}
</style></head>
<body><main>
<img src="snapper-logo.png" alt="Snapper">
<h1>Snapper</h1>
<p>{BLURB}</p>
{body}
<small>{foot} · GPL-3.0 · <a href="https://github.com/delirioux/snapper">Source</a> ·
Felucca by Leo Kuroshita, Hügelton Instruments</small>
</main></body></html>
"""


def waiting(out):
    """the start page before the first release: no package yet, the test build under dev/"""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / "docs/snapper-logo.png", out / "snapper-logo.png")
    dev = (out / "dev" / "index.html").exists()
    body = ('<p class="t">Not released yet: Snapper is being tested on the FM-1. The test build installs, and plays in the '
            'browser, from the dev branch.</p>'
            + ('<a class="b" href="dev/">The test build</a>' + ('<a class="b" href="dev/try/">Try it in the browser</a>'
                                                               if (out / "dev" / "try").exists() else "") if dev else ""))
    (out / "index.html").write_text(page(body, f"Snapper {sn_version()}, not yet released"), encoding="utf-8")
    print(f"site: {out}: waiting for the first release" + (", the test build under dev/" if dev else ""))


def main(pkg, build, out):
    version = f"{sn_version()} ({build})"
    test = build.startswith("dev-")                 # a build from dev: said so on both pages
    test_note = ('<p class="t">A test build from the dev branch. Snapper is not released yet: once a test build has run '
                 'on the FM-1, the release goes on <a href="../">the main page</a>.</p>' if test else "")
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca-built package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca's loader: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages")
    html = (HERE / "snapper_installer.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    name = f"snapper-{sn_version()}-{re.sub(r'[^A-Za-z0-9.-]', '-', build)}.fwsc"
    meta = json.dumps({"version": version, "felucca": felucca_version(), "product": product,
                       "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"snapper_installer.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, fw = out / "install", out / "firmware"
    for d in (inst, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("snapper-*.fwsc"):              # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    lic = ROOT / "LICENSES"
    (fw / "LICENSES").mkdir(exist_ok=True)
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Snapper licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    shutil.copy(ROOT / "docs/snapper-logo.png", out / "snapper-logo.png")
    emu = ROOT / os.environ.get("SN_EMU_DIR", "build/sn_emu")   # (the browser build of this package's branch)
    try_link = ""
    if (emu / "snapper.wasm").exists() and os.environ.get("SN_SITE_TRY") != "0":
        dst = out / "try"
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(emu, dst, ignore=shutil.ignore_patterns("obj", "shim"))
        try_link = '<a class="b" href="try/">Try it in the browser</a>'
    if not test and (out / "dev" / "index.html").exists():
        try_link += '<br><a href="dev/">The test build from the dev branch</a>'
    (out / "index.html").write_text(page(test_note + '<a class="b" href="install/">Install on the FM-1</a>' + try_link,
                                         f"Snapper {version}, on Felucca {felucca_version()}"), encoding="utf-8")
    print(f"site: {out}: install ({len(html)} B), firmware/{name} ({len(raw)} B, {product})"
          + (", try" if try_link else ""))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--waiting":
        waiting(sys.argv[2])
    elif len(sys.argv) == 4:
        main(*sys.argv[1:4])
    else:
        sys.exit(__doc__)
