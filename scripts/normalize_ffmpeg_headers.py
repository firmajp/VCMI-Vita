# SPDX-License-Identifier: GPL-3.0-or-later
"""Vita ABI adaptation, 2026-10-30: keep public FFmpeg enum definitions int-sized."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1])
assert root.is_dir()
for p in root.rglob('*.h'):
    text=p.read_text()
    changed=re.sub(r'\benum\s+([A-Za-z_]\w*)\s*\{',r'enum __attribute__((mode(SI))) \1 {',text)
    if text != changed:
        p.write_text('/* Vita ABI adaptation, 2026-10-30: int-sized public enums. */\n'+changed)
