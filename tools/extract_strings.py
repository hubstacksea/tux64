#!/usr/bin/env python3
"""List the merged C string literals of the game sources (used to build the translation table)."""
import re, sys, json
def literals(src):
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith('//', i): i = src.find('\n', i); i = n if i < 0 else i; continue
        if src.startswith('/*', i): i = src.find('*/', i) + 2; continue
        if c == "'":
            j = i + 1
            while src[j] != "'": j += 2 if src[j] == '\\' else 1
            i = j + 1; continue
        if c == '"':
            parts = []
            while True:
                j = i + 1; buf = ''
                while src[j] != '"':
                    if src[j] == '\\':
                        e = src[j + 1]
                        buf += {'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'"}.get(e, '\\' + e); j += 2
                    else: buf += src[j]; j += 1
                parts.append(buf); i = j + 1
                k = i
                while k < n and src[k] in ' \t\r\n': k += 1
                if k < n and src[k] == '"': i = k; continue
                break
            out.append(''.join(parts)); continue
        i += 1
    return out
seen = []
for f in sys.argv[1:]:
    for s in literals(open(f, encoding='utf-8').read()):
        if re.search('[A-Za-z]{2}', s) and s not in seen: seen.append(s)
json.dump(seen, sys.stdout, ensure_ascii=False, indent=0)
