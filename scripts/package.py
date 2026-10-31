# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import zipfile,json,io
import sys
sys.dont_write_bytecode=True
from vpk_layout import flatten_license_paths
import argparse
repo=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--bare-vpk',type=Path,default=repo/'build/vcmi/clientapp/vcmiclient.vpk')
parser.add_argument('--output',type=Path,default=repo/'dist/vcmi_vita_1.0.0-rc.1.vpk')
args=parser.parse_args()
root=repo/'src/vcmi'; build=args.bare_vpk
out=args.output
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as dst:
 with zipfile.ZipFile(build) as src:
  for entry in src.infolist():dst.writestr(entry.filename,src.read(entry.filename))
 def bundle(directory):
  data=io.BytesIO()
  with zipfile.ZipFile(data,'w',zipfile.ZIP_STORED) as archive:
   for p in sorted(directory.rglob('*')):
    if p.is_file(): archive.write(p,p.relative_to(directory).as_posix())
  return data.getvalue()
 for p in (root/'scripts').rglob('*'):
  if p.is_file(): dst.write(p,p.relative_to(root).as_posix())
 dst.writestr('Mods/vcmi/Content.zip',bundle(root/'Mods/vcmi/Content'))
 for p in (root/'Mods/vcmi').glob('*'):
  if p.is_file(): dst.write(p,p.relative_to(root).as_posix())
 font=root/'Mods/vcmi/Content/Data/NotoSans-Medium.ttf'
 dst.write(font,font.relative_to(root).as_posix())
 dst.writestr('config.zip',bundle(root/'config'))
 filesystem=(root/'config/filesystem.json').read_text()
 needle='"CONFIG/":\n\t\t['
 assert needle in filesystem
 filesystem=filesystem.replace(needle,needle+'\n\t\t\t{"type" : "zip", "path" : "config.zip"},',1)
 dst.writestr('config/filesystem.json',filesystem)
 dst.write(repo/'packaging/vitaBoot.png','config/vitaBoot.png')
 for p in (repo/'packaging/sce_sys').rglob('*'):
  if p.is_file(): dst.write(p,p.relative_to((repo/'packaging')).as_posix())
 settings={'video':{'resolution':{'width':960,'height':544,'scaling':90},'cursor':'software','showIntro':False,'vsync':True,'targetfps':30}}
 dst.writestr('config/settings.json',json.dumps(settings,indent=2))
 for p in (repo/'LICENSES').rglob('*'):
  if p.is_file(): dst.write(p,'licenses/'+p.relative_to(repo/'LICENSES').as_posix())
 for name in ['LICENSE','THIRD_PARTY_NOTICES.md','SOURCE_MANIFEST.json','SOURCE_ACCESS.md','ARTWORK_PROVENANCE.md','MODIFICATIONS.md','PTHREAD_LINKING.md','PORT_PROVENANCE.md','BINARY_BUILD_PROVENANCE.json','VERSION.json']:
  dst.write(repo/name,'licenses/'+name)
flatten_license_paths(out)
print(out, out.stat().st_size)
