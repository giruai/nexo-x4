#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export exact patched dependency sources from an explicit upstream file inventory.

Inputs and generated metadata stay local; the archive records relative paths only.
The inventory selects upstream source files, never build caches or credentials.
"""
import argparse,gzip,hashlib,io,json,os,pathlib,subprocess,tarfile
p=argparse.ArgumentParser(description=__doc__)
for name in ['rtc','inventory','zlib','output']:p.add_argument('--'+name,required=True,type=pathlib.Path)
p.add_argument('--epoch',type=int,default=0)
a=p.parse_args();project=pathlib.Path(__file__).resolve().parents[1]
inventory=json.loads(a.inventory.read_text());files={}
def add(name,path):
    key=pathlib.PurePosixPath(name)
    if key.is_absolute() or '..' in key.parts:raise SystemExit('Unsafe source path')
    if not path.is_file():raise SystemExit('Missing source: '+name)
    files[str(key)]=(path.read_bytes(),0o755 if os.access(path,os.X_OK) else 0o644)
for record in inventory['files']:
    name=record['path']
    if name.startswith('vendor/') and not name.startswith('vendor/zlib/'):
        add(name,a.rtc/name.removeprefix('vendor/'))
for path in sorted((a.rtc/'overlay').rglob('*')):
    if path.is_file():add('overlay/'+path.relative_to(a.rtc/'overlay').as_posix(),path)
add('vendor/libdatachannel/src/rtc_receiver_nack.cpp',a.rtc/'libdatachannel/src/rtc_receiver_nack.cpp')
zlib_pin=subprocess.check_output(['git','-C',str(a.zlib),'rev-parse','HEAD'],text=True).strip()
if zlib_pin!='51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf':raise SystemExit('Unexpected zlib source pin')
if subprocess.check_output(['git','-C',str(a.zlib),'status','--porcelain']):raise SystemExit('zlib source is modified')
for raw in subprocess.check_output(['git','-C',str(a.zlib),'ls-files','-z']).split(b'\0'):
    if raw:
        name=os.fsdecode(raw);add('vendor/zlib/'+name,a.zlib/name)
for directory in ['scripts/webrtc','patches','docs/licenses']:
    for path in sorted((project/directory).rglob('*')):
        if path.is_file() and '__pycache__' not in path.parts:add(path.relative_to(project).as_posix(),path)
for name in ['scripts/common.sh','scripts/build-deps.sh','scripts/unpack-deps.py','scripts/export-deps.py','LICENSE','CREDITS.md','THIRD_PARTY_NOTICES.md']:add(name,project/name)
components=[c for c in inventory['components'] if c['path']!='vendor/zlib']+[dict(path='vendor/zlib',upstream='https://github.com/madler/zlib.git',commit=zlib_pin)]
manifest=dict(format=1,target='OpenOrbis PS4',version=(project/'VERSION').read_text().strip(),components=components,
    files=[dict(path=name,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)) for name,(data,mode) in sorted(files.items())])
files['SOURCE_MANIFEST.json']=(json.dumps(manifest,indent=2,sort_keys=True).encode()+b'\n',0o644)
files['SOURCE_SHA256SUMS']=(''.join(f'{r["sha256"]}  {r["path"]}\n' for r in manifest['files']).encode(),0o644)
prefix='Nexo-X4-v'+manifest['version']+'-dependency-sources'
with a.output.open('xb') as output,gzip.GzipFile(fileobj=output,mode='wb',filename='',mtime=0) as compressed,tarfile.open(fileobj=compressed,mode='w|') as archive:
    for name,(data,mode) in sorted(files.items()):
        info=tarfile.TarInfo(prefix+'/'+name);info.size=len(data);info.mode=mode;info.mtime=a.epoch
        info.uid=info.gid=0;info.uname=info.gname='';archive.addfile(info,io.BytesIO(data))
print(json.dumps(dict(filename=a.output.name,sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),files=len(files))))
