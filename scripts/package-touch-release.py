"""Package the independently downloadable, instance-ID-free touch tool."""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--tag',default='v2026.11.15-touch1')
args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent
if args.output.exists():parser.error('Use a new output directory.')
if args.output.resolve().is_relative_to(repo):parser.error('Output must be outside the source repository.')
if not args.tag or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._' for c in args.tag):parser.error('Invalid source tag.')
root=args.output/'03-Laptop-Finger-Touch';(root/'app').mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
pins={}
for name in ['TouchToggle.exe','TouchDispatcher.exe']:
    shutil.copy2(args.build/name,root/'app'/name);pins[name]=sha(root/'app'/name)
text=(repo/'touch-toggle/package/setup.ps1').read_text(encoding='utf-8-sig')
text=text.replace('@BACKEND_SHA256@',pins['TouchToggle.exe']).replace('@DISPATCHER_SHA256@',pins['TouchDispatcher.exe'])
assert '@BACKEND_SHA256@' not in text and '@DISPATCHER_SHA256@' not in text
(root/'setup.ps1').write_bytes(text.replace('\n','\r\n').encode('utf-8-sig'))
shutil.copy2(repo/'touch-toggle/package/먼저 읽기.txt',root/'먼저 읽기.txt')
shutil.copy2(repo/'LICENSE',root/'LICENSE')
source=f'''Finger touch shortcut tool / Samsung 940X3M / Windows x64
Source: https://github.com/wow2658/moonlight-pen/tree/{args.tag}
License: GPL-3.0. Copyright notices in corresponding source files and LICENSE.
Build: MSVC x64, static CRT, /O2 /MT /guard:cf, /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT.
Both initial import resolution and later DLL loads are restricted to System32.
Command: scripts\\build-touch-release.cmd OUTPUT_DIRECTORY
No private profile is compiled into this release. Runtime read-only enumeration
requires model 940X3M, exactly one STMD1234 COL01 finger collection and one
separate WCOM0032 COL02 pen collection with their respective ACPI parents.
The original one-shot transition retains the first 100ms wait and pen checks.
No binary in this package is code-signed. Security review scope: source inspection,
read-only hardware check, device-selection tests, setup simulations, hash/privacy
scans. Actual new-release OFF/ON and fresh Windows end-to-end remain untested.
'''
(root/'SOURCE.txt').write_bytes(source.replace('\n','\r\n').encode('utf-8'))
for name,mode in [('INSTALL.cmd','Install'),('CHECK.cmd','Check'),('UNINSTALL.cmd','Uninstall')]:
    text='@echo off\r\nsetlocal\r\n"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" -Mode '+mode+'\r\nset "setupExit=%errorlevel%"\r\npause\r\nexit /b %setupExit%\r\n'
    (root/name).write_bytes(text.encode('ascii'))
files=[{'path':p.relative_to(root).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(root.rglob('*')) if p.is_file()]
(root/'manifest.json').write_text(json.dumps({'sourceTag':args.tag,'supportedModel':'940X3M','containsPrivateInstanceIds':False,'files':files},indent=2)+'\n',encoding='utf-8',newline='\n')
paths=[p for p in sorted(root.rglob('*')) if p.is_file()]
(root/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.relative_to(root).as_posix()+'\n' for p in paths),encoding='utf-8',newline='\n')
archive=args.output/'03-Laptop-Finger-Touch.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(root.rglob('*')):
        if p.is_file():z.write(p,'03-Laptop-Finger-Touch/'+p.relative_to(root).as_posix())
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
(args.output/'03-Laptop-Finger-Touch-SHA256.txt').write_text(sha(archive)+'  '+archive.name+'\n',encoding='ascii',newline='\n')
print(json.dumps({'zip':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive),'pins':pins,'tag':args.tag}))
