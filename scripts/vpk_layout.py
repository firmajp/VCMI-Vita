# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep on-device VPK paths short; retain complete third-party notices."""
from pathlib import Path, PurePosixPath
import hashlib
import zipfile

def validate_install_paths(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(names))
        for name in names:
            parts = PurePosixPath(name).parts
            assert not name.startswith('/') and '..' not in parts, name
            assert len(parts) - 1 <= 4, name
            assert all(len(part.encode('utf-8')) <= 16 for part in parts[:-1]), name
            assert len(parts[-1].encode('utf-8')) <= 32, name

def flatten_license_paths(path):
    path = Path(path)
    with zipfile.ZipFile(path) as source:
        names = [n for n in source.namelist() if n.startswith('licenses/')]
        if 'licenses/index.txt' in names and all(n.count('/') == 1 for n in names):
            validate_install_paths(path)
            return
    temporary = path.with_suffix('.layout.tmp')
    preserved = {}
    index = ['Complete license/notice files. Original paths and SHA256 values follow.\n']
    with zipfile.ZipFile(path) as source, zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as target:
        names = sorted(n for n in source.namelist() if n.startswith('licenses/'))
        for number, name in enumerate(names, 1):
            if name.endswith('/'):
                continue
            data = source.read(name)
            if name.count('/') > 1:
                replacement = f'licenses/n{number:04d}.txt'
            else:
                replacement = name
            digest = hashlib.sha256(data).hexdigest()
            preserved[replacement] = digest
            target.writestr(replacement, data)
            index.append(f'{replacement}\n  Original: {name}\n  SHA256: {digest}\n')
        target.writestr('licenses/index.txt', '\n'.join(index))
        for info in source.infolist():
            if not info.filename.startswith('licenses/'):
                target.writestr(info, source.read(info.filename))
    validate_install_paths(temporary)
    with zipfile.ZipFile(temporary) as result:
        assert all(hashlib.sha256(result.read(n)).hexdigest() == h for n, h in preserved.items())
    temporary.replace(path)
