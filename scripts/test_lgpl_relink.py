# SPDX-License-Identifier: GPL-3.0-or-later
"""Source/rebuild/relink probe for LGPL 2.1 section 6(a); no device execution is claimed."""
from pathlib import Path
import hashlib,json,os,subprocess,shutil
root=Path(__file__).resolve().parents[1]
sdk=Path(os.environ['VITASDK'])
source=root/'third_party/pthread-embedded/pthread_mutex_lock.c'
library=sdk/'arm-vita-eabi/lib/libpthread.a'
build=root/'build/vcmi'
release=build/'clientapp/vcmiclient'
proof=root/'build/relink-proof'
proof.mkdir(parents=True,exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest()
original_source=source.read_bytes();original_library=library.read_bytes()
release_hash=sha(release.read_bytes())
version=json.loads((root/'VERSION.json').read_text())['version'].replace('.','_').replace('-','_')
symbol='vcmi_lgpl_relink_probe_'+version
marker='VCMI_'+version+'_MODIFIED_PTHREAD_RELINK_PROBE'
addition=('\n/* Temporary source/relink verification; LGPL-2.1 terms retained. */\n'
          f'const char * {symbol}(void) {{ return "{marker}"; }}\n').encode()
result={}
try:
    copy=proof/'pthread-source'
    shutil.copytree(source.parent,copy,dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git','build'))
    (copy/source.name).write_bytes(original_source+addition)
    flags=(f'-ffile-prefix-map={root.as_posix()}=/vcmi-vita -fdebug-prefix-map={root.as_posix()}=/vcmi-vita '
           f'-ffile-prefix-map={sdk.as_posix()}=/vitasdk -fdebug-prefix-map={sdk.as_posix()}=/vitasdk '
           '-Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types')
    subprocess.run(['cmake','-S',str(copy),'-B',str(proof/'pthread-build'),'-G','Ninja',
                    f'-DCMAKE_TOOLCHAIN_FILE={sdk.as_posix()}/share/vita.toolchain.cmake',
                    f'-DCMAKE_INSTALL_PREFIX={sdk.as_posix()}/arm-vita-eabi','-DBUILD_TESTSUITE=OFF',
                    '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_POSITION_INDEPENDENT_CODE=OFF','-DCMAKE_POLICY_VERSION_MINIMUM=3.5',f'-DCMAKE_C_FLAGS={flags}'],check=True)
    subprocess.run(['cmake','--build',str(proof/'pthread-build'),'--parallel','2'],check=True)
    subprocess.run(['cmake','--install',str(proof/'pthread-build')],check=True)
    modified_hash=sha(library.read_bytes())
    assert modified_hash!=sha(original_library)
    commands=subprocess.check_output(['ninja','-t','commands','vcmiclient'],cwd=build,text=True)
    token=' -o clientapp\\vcmiclient '
    links=[line for line in commands.splitlines() if token in line]
    assert len(links)==1, 'Expected one native client link command'
    command=links[0].replace(token,f' -Wl,-u,{symbol} -o ../relink-proof/vcmiclient-modified.elf ',1)
    # Ninja already supplies the correctly quoted native cmd.exe wrapper.
    subprocess.run(command,cwd=build,check=True)
    output=proof/'vcmiclient-modified.elf'
    symbols=subprocess.check_output([str(sdk/'bin/arm-vita-eabi-nm.exe'),str(output)],text=True)
    assert symbol in symbols and marker.encode() in output.read_bytes()
    result={'status':'passed','method':'Append a probe function in a separate copy of LGPL pthread_mutex_lock.c, rebuild/install the modified library, relink the complete application to a separate ELF with -Wl,-u forcing this symbol, verify symbol and literal marker, restore the original library. Release source is never modified.',
            'original_source_sha256':sha(original_source),'modified_source_sha256':sha(original_source+addition),
            'original_library_sha256':sha(original_library),'modified_library_sha256':modified_hash,
            'modified_application_sha256':sha(output.read_bytes()),'probe_symbol':symbol,
            'device_execution':'not performed','release_executable_sha256':release_hash}
finally:
    library.write_bytes(original_library)
    assert sha(release.read_bytes())==release_hash
    assert source.read_bytes()==original_source and library.read_bytes()==original_library
if result:
    result['original_source_and_library_restored']=True
    result['release_executable_unchanged']=True
    (proof/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
