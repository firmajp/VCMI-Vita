# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate distribution layout and nested VPK content; not a legal certification."""
from pathlib import Path, PurePosixPath
import sys
sys.dont_write_bytecode=True
from vpk_layout import validate_install_paths
import hashlib
import io
import json
import re
import struct
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
VPK = ROOT / 'dist/vcmi_vita_1.0.0-rc.1.vpk'
FORBIDDEN = {'.lod', '.snd', '.vid', '.h3m', '.h3c', '.mp3', '.psp2dmp', '.suprx', '.skprx'}

def entries(archive, prefix=''):
    assert archive.testzip() is None
    names = archive.namelist()
    assert len(names) == len(set(names)), 'Duplicate ZIP paths'
    for name in names:
        path = PurePosixPath(name)
        assert not path.is_absolute() and '..' not in path.parts
        assert path.suffix.lower() not in FORBIDDEN, prefix + name
        assert not any(part.lower() in {'saves', 'mp3', '.git'} for part in path.parts)
        data = archive.read(name)
        yield prefix + name, data
        if name.lower().endswith('.zip'):
            with zipfile.ZipFile(io.BytesIO(data)) as nested:
                yield from entries(nested, prefix + name + '!/')

validate_install_paths(VPK)
with zipfile.ZipFile(VPK) as archive:
    files = dict(entries(archive))
    assert 'eboot.bin' in files and 'sce_sys/param.sfo' in files
    excluded = set(json.loads((ROOT/'EXCLUDED_ARTWORK_SHA256.json').read_text()).values())
    assert not any(hashlib.sha256(data).hexdigest() in excluded for data in files.values()), 'Excluded artwork reintroduced'
    for relative in ['icon0.png','pic0.png','livearea/contents/bg.png','livearea/contents/startup.png']:
        assert files['sce_sys/'+relative] == (ROOT/'packaging/sce_sys'/relative).read_bytes()
    for required in ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'SOURCE_ACCESS.md', 'SOURCE_MANIFEST.json',
                     'PTHREAD_LINKING.md', 'PORT_PROVENANCE.md', 'GPL-3.0.txt', 'oneTBB-Apache-2.0.txt', 'pthread-LGPL-2.1.txt', 'OFL-NOTO.txt',
                     'CC-BY-SA-4.0.txt', 'CC0-1.0.txt', 'MODIFICATIONS.md', 'VCMI-ASSET-AUTHORS.txt', 'GCC-RUNTIME-EXCEPTION.txt', 'FFmpeg-LGPL-2.1.txt', 'FFmpeg-LICENSE.md', 'SDL-ARM-BLITTER-NOTICES.txt']:
        assert 'licenses/' + required in files, required
    for notice in ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'SOURCE_MANIFEST.json', 'SOURCE_ACCESS.md',
                   'ARTWORK_PROVENANCE.md', 'MODIFICATIONS.md', 'PTHREAD_LINKING.md', 'PORT_PROVENANCE.md']:
        assert files['licenses/' + notice] == (ROOT / notice).read_bytes(), ('Stale bundled notice', notice)
    sfo = files['sce_sys/param.sfo']
    magic, version, keys, data_start, count = struct.unpack_from('<5I', sfo)
    assert magic == 0x46535000
    values = {}
    for i in range(count):
        key, fmt, length, maximum, offset = struct.unpack_from('<HHIII', sfo, 20 + i * 16)
        end = sfo.index(b'\0', keys + key)
        values[sfo[keys + key:end].decode()] = sfo[data_start + offset:data_start + offset + length].rstrip(b'\0')
    assert values['TITLE_ID'] == b'H3VC00001' and values['APP_VER'] == b'01.00'
    assert values['TITLE'] == b'VCMI-Vita 1.0 RC1'
    template = ET.fromstring(files['sce_sys/livearea/contents/template.xml'])
    for node in ['livearea-background/image', 'gate/startup-image']:
        assert 'sce_sys/livearea/contents/' + template.find(node).text in files
    labels = json.loads(files['config.zip!/vitaLoading.json'])
    english = json.loads(files['Mods/vcmi/Content.zip!/config/translations/english.json'])
    assert len(labels) == 46 and all(english[k] == v for k, v in labels.items())
    for bundled, directory in [('config.zip!/', ROOT/'src/vcmi/config'),
                               ('Mods/vcmi/Content.zip!/', ROOT/'src/vcmi/Mods/vcmi/Content')]:
        expected = {p.relative_to(directory).as_posix(): p for p in directory.rglob('*') if p.is_file()}
        actual = {n[len(bundled):]: data for n, data in files.items() if n.startswith(bundled)}
        assert set(expected) == set(actual)
        assert all(p.read_bytes() == actual[n] for n, p in expected.items())

all_paths = [p for p in ROOT.rglob('*') if p.is_file() and p.relative_to(ROOT).parts[0] != 'build' and not any(
    part in {'.git', '__pycache__', '.cache'} for part in p.relative_to(ROOT).parts)]
oversize = [p.relative_to(ROOT).as_posix() for p in all_paths if p.stat().st_size >= 100_000_000]
assert not oversize, ('GitHub file size limit', oversize)
for p in all_paths:
    assert p.suffix.lower() not in FORBIDDEN, p
    if p.suffix.lower() in {'.md', '.ps1', '.py', '.cmake', '.json', '.txt'} and p.stat().st_size < 2_000_000:
        text = p.read_text(errors='replace')
        assert not re.search(r'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16}', text), p

result = {
    'vpk': VPK.name, 'sha256': hashlib.sha256(VPK.read_bytes()).hexdigest(),
    'vpk_bytes': VPK.stat().st_size, 'recursive_vpk_entries': len(files),
    'source_tree_files': len(all_paths), 'all_files_below_100MB': True,
    'checks': ['ZIP CRC/paths/duplicates', 'short on-device paths and bounded directory depth', 'nested original-data extension denylist',
               'required licenses/source notices', 'bundled notice byte equality', 'Vita title/version', 'LiveArea XML references',
               '46 translations', 'excluded artwork hashes', 'original geometric LiveArea', 'bundled source byte equality', 'common secret-token patterns'],
    'limits': ['No legal certification', 'Not a judicial determination of third-party rights',
               'Exact rebuilt RC device installation test remains pending', 'ScanCode findings are documented separately; no independent legal audit'],
}
(ROOT/'AUDIT_RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
