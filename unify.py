#!/bin/env python3
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Generates an agglomerated header out of various other headers.
"""

import argparse
import sys

from pathlib import Path

class Header:
  def __init__(self, path):
    self.path = path
    self.fulltext = path.read_text()

    lines = []
    self.includes = []

    has_seen_licence = False
    ifdef_guard = None
    for line in self.fulltext.split('\n'):
      if not has_seen_licence and line.startswith('//'):
        continue
      has_seen_licence = True

      if ifdef_guard == None and line.startswith('#ifndef'):
        ifdef_guard = line[len('#ifndef'):].strip()
      elif ifdef_guard != None and \
        (line == f'#define {ifdef_guard}' or \
          line == f'#endif  // {ifdef_guard}'):
        pass
      elif line.startswith('#include'):
        self.includes.append(line[len('#include'):].strip())
      else:
        lines.append(line)

    self.text = '\n'.join(lines).strip()


def main():
  parser = argparse.ArgumentParser(description='agglomerate some headers')
  parser.add_argument(
    '--guard',
    type=str,
    default='CWISSTABLE_H_',
    help='include guard name for the agglomerated header'
  )
  parser.add_argument(
    '--out',
    type=argparse.FileType('w', encoding='utf-8'),
    default='cwisstable.h',
    help='location to output the file to'
  )
  parser.add_argument(
    'hdrs',
    type=Path,
    nargs='+',
    help='headers to agglomerate'
  )
  args = parser.parse_args()

  hdrs = {}
  for hdr in args.hdrs:
    hdrs[hdr] = Header(hdr)

  hdr_names = {f'"{name}"' for name, _ in hdrs.items()}
  external_includes = set()
  internal_includes = set()
  for _, hdr in hdrs.items():
    for inc in hdr.includes:
      if inc in hdr_names:
        internal_includes.add(inc.strip('"'))
      else:
        external_includes.add(inc)

  roots = [hdr for _, hdr in hdrs.items() if str(hdr.path) not in internal_includes]
  roots.sort()  # This script must be idempotent.

  if not roots:
    print("dependency cycle detected")
    return 1
  
  # Toposort the include dependencies.
  unsorted = dict(hdrs)
  sorted = []
  while roots:
    hdr, roots = roots[0], roots[1:]
    sorted.append(hdr)
    del unsorted[hdr.path]

    for inc in hdr.includes:
      if inc not in hdr_names:
        continue

      for _, hdr in unsorted.items():
        if inc in hdr.includes: break
      else:
        roots.append(hdrs[Path(inc.strip('"'))])

  o = args.out

  # Add the license, cribbing from this file itself.
  for line in Path(__file__).read_text().split('\n')[1:]:
    if not line.startswith('#'):
      break
    o.write(line.replace('#', '//') + "\n")
  o.write("\n")

  o.write("// THIS IS A GENERATE FILE! DO NOT EDIT DIRECTLY!\n")
  o.write("// Generated using glob.py, by concatenating, in order:\n")
  for hdr in sorted[::-1]:
    o.write(f'// #include "{hdr.path}"\n')
  o.write("\n")

  # Add the include guards.
  o.write(f"#ifndef {args.guard}\n")
  o.write(f"#define {args.guard}\n")
  o.write("\n")

  # Add the external includes.
  external_includes = list(external_includes)
  external_includes.sort()
  for inc in external_includes:
    o.write(f"#include {inc}\n")
  o.write("\n")

  # Add each header.
  for hdr in sorted[::-1]:
    name = f"/// {hdr.path} /"
    name += '/' * (80 - len(name))
    o.write('\n'.join([name, hdr.text, name]) + "\n\n")

  # Add the include guard tail.
  o.write(f"#endif  // {args.guard}\n")

if __name__ == '__main__': sys.exit(main() or 0)