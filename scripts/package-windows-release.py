"""Package a freshly deployed runtime; never copy an in-use Moonlight folder.

Requires Python 3.9+. This builds local ZIPs and does not upload anything.
"""
from pathlib import Path
import argparse
import hashlib
import shutil
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True, help='Fresh windeployqt output containing Moonlight.exe')
parser.add_argument('--output', type=Path, required=True, help='New output directory outside the source checkout')
parser.add_argument('--tag', required=True, help='Release tag providing the corresponding source')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
runtime = args.runtime.resolve()
output = args.output.resolve()
if not (runtime / 'Moonlight.exe').is_file():
    parser.error('--runtime must contain Moonlight.exe')
if output.exists():
    parser.error('--output must be a new directory')
if root == output or root in output.parents:
    parser.error('--output must be outside the source checkout')
if not args.tag or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-' for c in args.tag):
    parser.error('--tag must contain only letters, digits, dot, underscore, or hyphen')

blocked = {'.ini', '.log', '.key', '.pem', '.pfx', '.p12', '.crt', '.cer', '.pdb', '.dmp', '.lnk', '.zip', '.7z'}
blocked_dirs = {'.git', '.ssh', 'cache', 'boxart', 'qmlcache', 'Moonlight Game Streaming Project'}
for path in runtime.rglob('*'):
    rel = path.relative_to(runtime)
    if path.is_symlink() or any(part.casefold() in {x.casefold() for x in blocked_dirs} for part in rel.parts):
        parser.error('Runtime contains a link or personal data directory: ' + rel.as_posix())
    if not path.is_file():
        continue
    if path.suffix.lower() in blocked or path.name.lower() in {'auth.json', '.env', 'portable.dat'}:
        parser.error('Runtime contains excluded data: ' + rel.as_posix())
    if path.suffix.lower() == '.exe' and rel.as_posix() != 'Moonlight.exe':
        parser.error('Unexpected executable: ' + rel.as_posix())
    if '140d' in path.name.lower():
        parser.error('Debug runtime must not be packaged: ' + rel.as_posix())

client = output / '01-Laptop-Moonlight-Pen'
host = output / '02-Host-Display-Shortcuts'
output.mkdir(parents=True)
shutil.copytree(runtime, client / 'app')
(client / 'app' / 'portable.dat').write_bytes(b'')

def copy_text(source, target, encoding='utf-8-sig'):
    target.parent.mkdir(parents=True, exist_ok=True)
    text = source.read_text(encoding='utf-8-sig')
    target.write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode(encoding))

copy_text(root / 'packaging/client/START-MOONLIGHT.cmd', client / '문라이트 실행.cmd', 'ascii')
copy_text(root / 'packaging/client/DELETE.cmd', client / '삭제.cmd', 'ascii')
uninstaller = (root / 'packaging/client/uninstall-client.ps1').read_text(encoding='utf-8-sig')
uninstaller = uninstaller.replace('@MOONLIGHT_SHA256@', hashlib.sha256((client / 'app/Moonlight.exe').read_bytes()).hexdigest())
(client / 'support').mkdir(exist_ok=True)
(client / 'support/uninstall-client.ps1').write_bytes(uninstaller.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8-sig'))
copy_text(root / 'packaging/client/먼저 읽기.txt', client / '00-먼저 읽기.txt')
copy_text(root / 'packaging/host/먼저 읽기.txt', host / '00-먼저 읽기.txt')
for source, target in [('INSTALL.cmd', '호스트 설치.cmd'), ('OPEN.cmd', '설치된 바로가기 열기.cmd'), ('UNINSTALL.cmd', '호스트 바로가기 삭제.cmd')]:
    copy_text(root / 'packaging/host' / source, host / target, 'ascii')
copy_text(root / 'packaging/host/UNINSTALL.cmd', host / '삭제.cmd', 'ascii')
for source in (root / 'host-shortcuts').iterdir():
    if source.is_file():
        copy_text(source, host / 'support' / source.name, 'ascii' if source.suffix in {'.cmd', '.vbs'} else 'utf-8-sig')
shutil.copytree(root / 'packaging/licenses', client / 'licenses')
shutil.copyfile(root / 'LICENSE', host / 'LICENSE')
source_note = (
    'Moonlight Pen: unofficial Windows build\n'
    f'Corresponding source: https://github.com/wow2658/moonlight-pen/tree/{args.tag}\n'
    f'Source archive: https://github.com/wow2658/moonlight-pen/archive/refs/tags/{args.tag}.zip\n'
    'Build instructions: BUILDING.md in the corresponding source.\n'
    'Original project: https://github.com/moonlight-stream/moonlight-qt\n'
    'Community pen fork: https://github.com/AaaUpo/moonlight-qt-surface\n'
    'License: GNU GPL v3. Third-party notices: licenses/.\n'
    'Dependency source/build references: licenses/SOURCE-REFERENCES.json.\n'
    'Source and downloads are public; GitHub sign-in is not required.\n'
)
(client / 'SOURCE.txt').write_text(source_note, encoding='utf-8')
(host / 'SOURCE.txt').write_text(source_note.replace('Third-party notices: licenses/.', 'See LICENSE; this package contains only text scripts.').replace('Dependency source/build references: licenses/SOURCE-REFERENCES.json.\n', ''), encoding='utf-8')
for folder in [client, host]:
    if folder == client:
        import json
        (folder / 'UNINSTALL-FILES.json').write_text(json.dumps({'format':1,'product':'Moonlight-Pen-portable','files':[p.relative_to(folder).as_posix() for p in sorted(folder.rglob('*')) if p.is_file()]},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    files = sorted(p for p in folder.rglob('*') if p.is_file())
    manifest = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(folder).as_posix() + '\n' for p in files)
    (folder / 'FILES-SHA256.txt').write_text(manifest, encoding='utf-8')
    archive = output / (folder.name + '.zip')
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for path in sorted(p for p in folder.rglob('*') if p.is_file()):
            zipped.write(path, path.relative_to(output).as_posix())
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise RuntimeError('Archive integrity test failed')
    print(archive.name, archive.stat().st_size, 'bytes')
archives = sorted(output.glob('*.zip'))
(output / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in archives), encoding='utf-8')
print('Packages and checksums created. Scan and verify before uploading.')
