# SPDX-License-Identifier: GPL-3.0-or-later
"""Check RC executable privacy, indexed artwork and shipped executable equality."""
from pathlib import Path
import hashlib,json,re,struct,zipfile,zlib,subprocess,os
root=Path(__file__).resolve().parents[1]
build_root=Path(os.environ.get('VCMI_AUDIT_BUILD_ROOT',root/'build'))
elf=build_root/'vcmi/clientapp/vcmiclient'
vpk=root/'dist/vcmi_vita_1.0.0-rc.1.vpk'
data=elf.read_bytes()
assert data[:6]==b'\x7fELF\x01\x01'
private=re.compile(rb'[A-Za-z]:[/\\]Users[/\\]|Documents[/\\]Codex',re.I)
def clean(payload,where):
    assert not private.search(payload), ('Private build path',where)
clean(data,'ELF')
phoff=struct.unpack_from('<I',data,28)[0]
entsize,count=struct.unpack_from('<HH',data,42)
segments=0
for i in range(count):
    kind,offset,va,pa,size,memsize,flags,align=struct.unpack_from('<8I',data,phoff+i*entsize)
    if kind==1:
        clean(data[offset:offset+size],f'load segment {i}');segments+=1
with zipfile.ZipFile(vpk) as z:
    eboot=z.read('eboot.bin')
    assert eboot==(build_root/'vcmi/clientapp/vcmiclient.self').read_bytes()
    clean(eboot,'SELF raw bytes')
    streams=0
    for match in re.finditer(rb'\x78[\x01\x5e\x9c\xda]',eboot):
        decoder=zlib.decompressobj()
        try:expanded=decoder.decompress(memoryview(eboot)[match.start():],128*1024*1024)
        except zlib.error:continue
        if decoder.eof:
            clean(expanded,f'SELF zlib stream at {match.start()}');streams+=1
    assert streams>0,'Expected compressed SELF segments'
    for name,dimensions in {'icon0.png':(128,128),'pic0.png':(960,544),'livearea/contents/bg.png':(840,500),'livearea/contents/startup.png':(280,158)}.items():
        image=z.read('sce_sys/'+name)
        assert struct.unpack_from('>II',image,16)==dimensions and image[25]==3
        position=8;colors=0
        while position<len(image):
            size=struct.unpack_from('>I',image,position)[0]
            if image[position+4:position+8]==b'PLTE':colors=size//3
            position+=12+size
        assert 0<colors<=128,(name,colors)
    assert z.read('config/vitaBoot.png')==(root/'packaging/vitaBoot.png').read_bytes()
sdk=Path(os.environ['VITASDK'])
symbols=subprocess.check_output([str(sdk/'bin/arm-vita-eabi-nm.exe'),str(elf)],text=True)
assert not re.search(r'\b(?:SSL_|EVP_|SSLeay|OPENSSL_|CRYPTO_)\w*',symbols)
for marker in [b'VCMI VITA BUILD 01.00 (1.0.0-rc.1)',b'vcmi-startup-rc1.log',b'vcmi-watchdog-rc1.log',b'vcmi-performance-rc1.log',b'town_cache hits=']:
    assert marker in data,marker
assert b'VCMI_RC1_MODIFIED_PTHREAD_RELINK_PROBE' not in data
result={'elf_sha256':hashlib.sha256(data).hexdigest(),'eboot_sha256':hashlib.sha256(eboot).hexdigest(),
        'private_paths_in_ELF_and_SELF':'none detected','ELF_load_segments_checked':segments,'SELF_valid_zlib_streams_checked':streams,
        'shipped_SELF_equal_to_clean_build':True,'indexed_LiveArea_dimensions_palette':'passed','boot_image_equality':True,'known_public_CI_paths':sorted(set(x.decode('utf-8',errors='replace') for x in re.findall(rb'/home/runner/[^\x00\r\n ]{1,300}',data))),
        'OpenSSL_implementation_symbols':'none detected','modified_pthread_probe_absent_from_release':True,
        'limits':'Technical pattern, format and equality checks; not a legal certificate or device execution test.'}
out=root/'docs/compliance/BINARY_CHECKS.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
