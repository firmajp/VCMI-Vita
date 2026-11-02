# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate final exact-byte SHA256 checksums or verify disk and optionally Git index blobs."""
from pathlib import Path
import argparse,hashlib,subprocess
root=Path(__file__).resolve().parents[1]
manifest=root/'SHA256SUMS'
parser=argparse.ArgumentParser()
parser.add_argument('mode',choices=['generate','verify'])
parser.add_argument('--git',action='store_true',help='Also verify indexed Git blobs; run git add first.')
args=parser.parse_args()
def sha(data):return hashlib.sha256(data).hexdigest()
def files():
    return sorted(p for p in root.rglob('*') if p.is_file() and p!=manifest
                  and p.relative_to(root).parts[0] != 'build'
                  and not (p.relative_to(root).parts[0] == 'dist' and (p.name.endswith('.vpk') or p.name.endswith('.vpk.sha256')))
                  and not any(part in {'.git','__pycache__','.cache'} for part in p.relative_to(root).parts))
if args.mode=='generate':
    manifest.write_text(''.join(sha(p.read_bytes())+'  '+p.relative_to(root).as_posix()+'\n' for p in files()),encoding='utf-8',newline='\n')
    print('Generated checksums:',len(files()))
else:
    expected={name:digest for digest,name in (line.split('  ',1) for line in manifest.read_text().splitlines())}
    actual={p.relative_to(root).as_posix():p for p in files()}
    assert set(expected)==set(actual),('Missing/extra files',set(expected)^set(actual))
    for name,p in actual.items():assert sha(p.read_bytes())==expected[name],('File mismatch',name)
    if args.git:
        process=subprocess.Popen(['git','cat-file','--batch'],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        for name,digest in expected.items():
            process.stdin.write((':0:'+name+'\n').encode());process.stdin.flush()
            header=process.stdout.readline().decode().split()
            assert len(header)==3 and header[1]=='blob',('Unindexed file',name)
            data=process.stdout.read(int(header[2]));assert process.stdout.read(1)==b'\n'
            assert sha(data)==digest,('Git blob mismatch',name)
        process.stdin.close();assert process.wait()==0
    print('Verified exact bytes:',len(expected),'files','and Git index' if args.git else '')
