"""Shared helpers for the golden corpus scripts (ADR 0010).

The template pack and everything generated from it are private (ADR 0008): they live beside the
pack under templates/, never in this repo's history.
"""

from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_PACK = REPO / "templates" / "Full-Magic-Pack"
DEFAULT_GOLDENS = REPO / "templates" / "goldens"
DEFAULT_MSE = REPO / "build" / "magicseteditor"

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def read_mse_file(path: Path) -> str:
    """An MSE data file as text (they are UTF-8, often with a byte order mark)."""
    return path.read_text(encoding="utf-8-sig")


def read_with_includes(path: Path, data_dir: Path, depth: int = 0) -> str:
    """An MSE data file with its top-level `include file:` lines replaced by the included text.

    `/pkg.mse-include/file` is relative to the data directory, a bare name to the including package.
    """
    if depth > 20:
        raise RuntimeError(f"include nesting too deep at {path}")
    out = []
    for line in read_mse_file(path).splitlines():
        m = re.match(r"^include file:\s*(.+?)\s*$", line)
        if not m:
            out.append(line)
            continue
        name = m.group(1)
        target = data_dir / name.lstrip("/") if name.startswith("/") else path.parent / name
        if target.is_file():
            out.append(read_with_includes(target, data_dir, depth + 1))
    return "\n".join(out)


def top_level_blocks(text: str, key: str) -> list[str]:
    """Every top-level block that starts with `key:`, including its indented body."""
    blocks = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if re.match(rf"^{re.escape(key)}\s*:", lines[i]):
            j = i + 1
            # the body is indented; blank lines and column-0 comments don't end it
            while j < len(lines) and (
                lines[j].startswith("\t") or not lines[j].strip() or lines[j].startswith("#")
            ):
                j += 1
            blocks.append("\n".join(lines[i:j]))
            i = j
        else:
            i += 1
    return blocks


def block_value(block: str, key: str) -> str | None:
    """The one-line value of a direct child `key:` of a block (indented by one tab)."""
    m = re.search(rf"^\t{re.escape(key)}\s*:[ \t]*(.*)$", block, re.M)
    return m.group(1).strip() if m else None


def fontconfig_file(pack: Path | None, out_dir: Path) -> Path:
    """A fontconfig file that adds the pack's fonts to the system ones, written into out_dir.

    The pack's Magic fonts are not installed system-wide; without them text falls back to
    another font and the layout changes. With no pack: the system fonts only.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    conf = out_dir / "fonts.conf"
    pack_dir = f"  <dir>{pack / 'Magic - Fonts'}</dir>\n" if pack else ""
    conf.write_text(
        '<?xml version="1.0"?>\n'
        '<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
        "<fontconfig>\n"
        '  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>\n'
        f"{pack_dir}"
        f"  <cachedir>{out_dir / 'fontconfig-cache'}</cachedir>\n"
        "</fontconfig>\n",
        encoding="utf-8",
    )
    return conf


def mse_env(pack: Path, out_dir: Path, templates: Path | None = None, font_store: Path | None = None) -> dict[str, str]:
    """The environment for MSE with the pack's Templates and fonts.

    With `templates` (and `font_store`): a Template store made by Template import instead. Then
    fontconfig knows only the system fonts, and the Magic fonts are found only because MSE loads
    the files in the shared font store (MSE_FONT_STORE) itself.
    """
    env = dict(os.environ)
    if templates is not None:
        env["MSE_TEMPLATES_DIR"] = str(templates)
        env["FONTCONFIG_FILE"] = str(fontconfig_file(None, out_dir / "imported-fontconfig"))
        if font_store is not None:
            env["MSE_FONT_STORE"] = str(font_store)
        else:
            env.pop("MSE_FONT_STORE", None)
        return env
    env["MSE_TEMPLATES_DIR"] = str(pack / "data")
    env["FONTCONFIG_FILE"] = str(fontconfig_file(pack, out_dir))
    # the same directory as the font registry's shared font store; MSE also loads its files for wx,
    # which finds them through the <dir> above anyway
    env["MSE_FONT_STORE"] = str(pack / "Magic - Fonts")
    return env


@dataclass
class CliResult:
    status: str  # "ok", "warning", "error", "timeout", "crash"
    returncode: int | None
    output: str


def run_cli(mse: Path, set_path: Path, commands: str, env: dict[str, str], timeout: float) -> CliResult:
    """Run `magicseteditor --cli -q <set>` with the given commands on stdin, and classify it."""
    try:
        proc = subprocess.run(
            [str(mse), "--cli", "-q", str(set_path)],
            input=commands,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as e:
        out = e.stdout or ""
        if isinstance(out, bytes):
            out = out.decode("utf-8", "replace")
        return CliResult("timeout", None, ANSI.sub("", out))
    output = ANSI.sub("", proc.stdout + proc.stderr)
    if proc.returncode < 0:
        status = "crash"
    elif proc.returncode != 0 or "ERROR" in output:
        status = "error"
    elif "WARNING" in output:
        status = "warning"
    else:
        status = "ok"
    return CliResult(status, proc.returncode, output)


def mse_string(s: str) -> str:
    """A string literal for an MSE script ({...} would otherwise be interpolated)."""
    for c in ("\\", '"', "{", "}"):
        s = s.replace(c, "\\" + c)
    return '"' + s + '"'
