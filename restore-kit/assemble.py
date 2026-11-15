"""Assemble a PRIVATE restore kit outside the Git checkout; no program is run."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, shutil, stat, zipfile

parser=argparse.ArgumentParser(description=__doc__)
for name in ['client-zip','host-zip','sunshine-msi','touch-backend','touch-dispatcher','touch-profile','output']:
    parser.add_argument('--'+name,required=True,type=Path)
args=parser.parse_args()
template=Path(__file__).resolve().parent
repository=template.parent
output=args.output.resolve()
if output.is_relative_to(repository) or output.exists():
    parser.error('Output must be a NEW directory outside this repository.')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
profile=json.loads(args.touch_profile.read_text(encoding='utf-8-sig'))
for key in ['touchId','touchParent','penId','penParent']:
    if not isinstance(profile.get(key),str) or not profile[key]:parser.error('Verified local device profile is incomplete.')
if sha(args.touch_backend).lower()!=profile.get('backendSha256','').lower():parser.error('Backend pin mismatch.')
if sha(args.touch_dispatcher).lower()!=profile.get('dispatcherSha256','').lower():parser.error('Receiver pin mismatch.')
if sha(args.sunshine_msi)!='1d7fed8beecd5889dc7ff14cf9f42d6d38f37c3066c13c6c2a5f4e91847e0ccf':
    parser.error('Expected the verified official Sunshine 2026.914.233613 AMD64 MSI.')
blocked={'.ini','.conf','.key','.pem','.pfx','.p12','.crt','.cer','.log','.lnk','.pdb','.dmp'}
def extract_clean(archive,prefix,destination):
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:raise ValueError('Archive CRC error.')
        for item in z.infolist():
            if item.is_dir():continue
            name=PurePosixPath(item.filename)
            if '\\' in item.filename or name.is_absolute() or '..' in name.parts or not item.filename.startswith(prefix):raise ValueError('Unsafe archive path.')
            if stat.S_ISLNK(item.external_attr>>16):raise ValueError('Archive links are not allowed.')
            relative=PurePosixPath(item.filename[len(prefix):])
            if ':' in str(relative):raise ValueError('Windows drive/stream paths are not allowed.')
            if relative.suffix.lower() in blocked or any(p.lower() in {'.git','.ssh','moonlight game streaming project','cache','boxart'} for p in relative.parts):
                raise ValueError('The input contains runtime/private data; use the clean distribution ZIP.')
            if len(relative.parts)==1 and relative.name not in {'SOURCE.txt','LICENSE','package.json','sunshine.exe','install.ps1','validation.ps1'}:continue
            data=z.read(item)
            if relative.name=='portable.dat' and data:raise ValueError('Portable marker must be empty.')
            dest=destination/str(relative)
            if not dest.resolve().is_relative_to(destination.resolve()):raise ValueError('Archive path escapes its destination.')
            dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
shutil.copytree(template,output,ignore=shutil.ignore_patterns('__pycache__','settings-backup*.json','device-profile.local.json','manifest.json','*-result.txt','*-output.txt','*-error.txt'))
extract_clean(args.client_zip,'01-Laptop/',output/'01-Laptop/client')
extract_clean(args.host_zip,'02-Host/',output/'02-Host/clipboard')
manifest=json.loads((output/'02-Host/clipboard/package.json').read_text(encoding='utf-8-sig'))
if sha(output/'01-Laptop/client/app/Moonlight.exe')!=manifest['clientSha256']:raise ValueError('Client executable differs from the matching host package.')
if sha(output/'02-Host/clipboard/sunshine.exe')!=manifest['patchedSha256']:raise ValueError('Host executable hash mismatch.')
(output/'02-Host/official').mkdir()
shutil.copy2(args.sunshine_msi,output/'02-Host/official/Sunshine-Windows-AMD64-installer.msi')
shutil.copytree(repository/'host-shortcuts',output/'02-Host/display-shortcuts')
(output/'03-Touch/app').mkdir()
shutil.copy2(args.touch_backend,output/'03-Touch/app/TouchToggle.exe')
shutil.copy2(args.touch_dispatcher,output/'03-Touch/app/TouchDispatcher.exe')
(output/'03-Touch/device-profile.local.json').write_text(json.dumps(profile,indent=2)+'\n',encoding='utf-8')
files=[{'path':p.relative_to(output).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(output.rglob('*')) if p.is_file()]
(output/'manifest.json').write_text(json.dumps({'version':'text-r3-touch-receiver','files':files,'privateKit':True,'hostSettingsCaptured':False},indent=2)+'\n',encoding='utf-8')
print('Assembled private kit:',output)
print('No install/device/service action was performed. Keep the whole kit private; back up host settings before formatting.')
