#!/usr/bin/env python3
"""clean.py -- remove the build products.

The Makefile cannot rely on rm(1): on some machines the shell that runs the
build has a rm shim on its path that is not a working program.  Python is
already a build requirement, so the removal is done here.
"""
import os
import shutil

TARGETS = [
    "build",
    "morphdemo-64k", "morphdemo-64k.elf",
    "morphdemo-16k", "morphdemo-16k.elf",
    "morphdemo-4k", "morphdemo-4k.elf",
    "tools/env.mk",
]

for path in TARGETS:
    if os.path.isdir(path) and not os.path.islink(path):
        shutil.rmtree(path, ignore_errors=True)
        print("removed %s/" % path)
    elif os.path.exists(path) or os.path.islink(path):
        try:
            os.remove(path)
            print("removed %s" % path)
        except OSError as e:
            print("could not remove %s: %s" % (path, e))
