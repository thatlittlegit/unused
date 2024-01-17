<!-- Copyright 2024 Duncan McIntosh
   - SPDX-License-Identifier: GPL-3.0-only
   -->
# unused
Unused searches for unused functions in an executable by examining the
disassembly.  It uses either [BFD](https://sourceware.org/binutils/)
or [bddisasm](https://github.com/bitdefender/bddisasm) to parse object
files, and reduces the list of functions until only unused ones
remain.

To build, use Meson:

```sh
meson setup build # or another directory name
meson compile -C build
```

Unused is licensed under the [GNU General Public License
3.0](COPYING).

