#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extract a release dependency bundle into a new isolated root and verify hashes."""
import hashlib,json,pathlib,shutil,sys,tarfile,tempfile
archive,destination=map(pathlib.Path,sys.argv[1:3])
if destination.exists() and any(destination.iterdir()):
    raise SystemExit('Refusing a non-empty dependency root without a source receipt')
destination.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='.nexo-deps-',dir=destination.parent) as temporary:
    staging=pathlib.Path(temporary)
    with tarfile.open(archive) as stream:
        stream.extractall(staging,filter='data')
    roots=list(staging.iterdir())
    if len(roots)!=1 or not roots[0].is_dir():raise SystemExit('Expected one archive root')
    source=roots[0];manifest=json.loads((source/'SOURCE_MANIFEST.json').read_text())
    for record in manifest['files']:
        p=source/record['path']
        if not p.resolve().is_relative_to(source.resolve()):raise SystemExit('Escaping source path')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=record['sha256']:raise SystemExit('Source checksum mismatch: '+record['path'])
    for component in ['libdatachannel','mbedtls','opus-1.5.2','zlib']:
        shutil.copytree(source/'vendor'/component,destination/component)
    shutil.copytree(source/'overlay',destination/'overlay')
    shutil.copy2(source/'SOURCE_MANIFEST.json',destination/'SOURCE_MANIFEST.json')
print('Dependency source files verified and extracted')
