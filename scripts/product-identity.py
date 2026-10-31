#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Print the three validated package identity fields, never evaluate the header."""
import pathlib, re
header = (pathlib.Path(__file__).resolve().parents[1] / 'src/core/product_identity.h').read_text()
def field(name):
    values = re.findall(r'^#define ' + name + r' "([^"\n]+)"$', header, re.M)
    if len(values) != 1: raise SystemExit('Invalid identity field: ' + name)
    return values[0]
title, tid, cid = [field(n) for n in ('NEXO_PRODUCT_TITLE', 'NEXO_TITLE_ID', 'NEXO_CONTENT_ID')]
if not re.fullmatch(r'[A-Za-z0-9 ]{1,32}', title): raise SystemExit('Invalid title')
if not re.fullmatch(r'[A-Z]{4}[0-9]{5}', tid) or tid in ('XCLD00001', 'XCLD00003'): raise SystemExit('Invalid or colliding title ID')
if not re.fullmatch(r'[A-Z]{2}[0-9]{4}-' + tid + r'_[0-9]{2}-[A-Z0-9]{16}', cid): raise SystemExit('Invalid content ID')
print(title, tid, cid, sep='\n')
