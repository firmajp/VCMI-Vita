# SPDX-License-Identifier: GPL-3.0-or-later
"""Normalize private paths only in the generated FFmpeg configuration-description string."""
from pathlib import Path
import sys
config=Path(sys.argv[1]);source=config.read_text()
lines=[]
for line in source.splitlines(keepends=True):
    if line.startswith('#define FFMPEG_CONFIGURATION '):
        for old,new in [(sys.argv[2],'/vcmi-vita'),(sys.argv[3],'/vitasdk')]:
            line=line.replace(old,new).replace(old.replace('/','\\'),new)
    lines.append(line)
config.write_text(''.join(lines))
