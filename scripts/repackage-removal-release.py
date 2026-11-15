"""Add one deletion entry to the existing clean 01/02 ZIPs; runtime unchanged."""
from pathlib import Path
import argparse,hashlib,json,subprocess,zipfile
parser=argparse.ArgumentParser(description=__doc__)
for name in ['client-zip','host-zip','touch-build','output']:
    parser.add_argument('--'+name,required=True,type=Path)
parser.add_argument('--tag',required=True)
args=parser.parse_args();repo=Path(__file__).resolve().parent.parent
if args.output.exists() or args.output.resolve().is_relative_to(repo):parser.error('Use a new output directory outside the source repository.')
if not args.tag or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-' for c in args.tag):parser.error('Invalid tag.')
sha=lambda data:hashlib.sha256(data).hexdigest()
args.output.mkdir(parents=True)
def text(relative):return (repo/relative).read_text(encoding='utf-8-sig').replace('\r\n','\n')
def encoded(value):return value.replace('\n','\r\n').encode('utf-8-sig')
def load(archive,prefix):
    result={}
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for info in z.infolist():
            if info.is_dir():continue
            assert info.filename.startswith(prefix)
            relative=info.filename[len(prefix):]
            assert relative and ':' not in relative and '\\' not in relative and '..' not in relative.split('/')
            assert Path(relative).suffix.lower() not in {'.ini','.pem','.key','.pfx','.p12','.crt','.cer','.log','.lnk','.pdb'}
            assert 'Moonlight Game Streaming Project' not in relative.split('/')
            result[relative]=z.read(info)
    return result
for kind,archive in [('01-Laptop-Moonlight-Pen',args.client_zip),('02-Host-Display-Shortcuts',args.host_zip)]:
    data=load(archive,kind+'/');data.pop('FILES-SHA256.txt',None)
    if kind.startswith('01'):
        before={p:sha(value) for p,value in data.items() if p.startswith('app/')}
        data['삭제.cmd']=text('packaging/client/DELETE.cmd').replace('\n','\r\n').encode('ascii')
        code=text('packaging/client/uninstall-client.ps1').replace('@MOONLIGHT_SHA256@',sha(data['app/Moonlight.exe']))
        data['support/uninstall-client.ps1']=encoded(code)
        data['00-먼저 읽기.txt']=encoded(text('packaging/client/먼저 읽기.txt'))
        data['UNINSTALL-FILES.json']=(json.dumps({'format':1,'product':'Moonlight-Pen-portable','files':sorted(data)},ensure_ascii=False,indent=2)+'\n').encode('utf-8')
        assert before=={p:sha(value) for p,value in data.items() if p.startswith('app/')}
    else:
        data['삭제.cmd']=text('packaging/host/UNINSTALL.cmd').replace('\n','\r\n').encode('ascii')
        data['00-먼저 읽기.txt']=encoded(text('packaging/host/먼저 읽기.txt'))
        data['support/uninstall-shortcuts.vbs']=text('host-shortcuts/uninstall-shortcuts.vbs').replace('\n','\r\n').encode('ascii')
    data['SOURCE.txt']+=('\nRemoval scripts corresponding source: https://github.com/wow2658/moonlight-pen/tree/'+args.tag+'\nThe existing runtime source reference above is preserved; program binaries are unchanged.\n').encode('utf-8')
    data['FILES-SHA256.txt']=''.join(sha(value)+'  '+name+'\n' for name,value in sorted(data.items())).encode('utf-8')
    target=args.output/(kind+'.zip')
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,value in sorted(data.items()):z.writestr(kind+'/'+name,value)
    with zipfile.ZipFile(target) as z:assert z.testzip() is None
subprocess.run([__import__('sys').executable,str(repo/'scripts/package-touch-release.py'),'--build',str(args.touch_build),'--output',str(args.output/'touch-package'),'--tag',args.tag],check=True)
for name in ['03-Laptop-Finger-Touch.zip','03-Laptop-Finger-Touch-SHA256.txt']:
    (args.output/name).write_bytes((args.output/'touch-package'/name).read_bytes())
archives=sorted(args.output.glob('*.zip'))
(args.output/'SHA256SUMS.txt').write_text(''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in archives),encoding='ascii',newline='\n')
print(json.dumps({'tag':args.tag,'runtimeChanged':False,'files':[{'name':p.name,'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in archives]}))
