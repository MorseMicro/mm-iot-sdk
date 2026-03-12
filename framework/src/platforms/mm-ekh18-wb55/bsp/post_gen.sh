#!/usr/bin/env sh

## Post processing script for CubeMX code generator

# Remove unused generated code
[ -d "Drivers/" ] && rm -r Drivers/

# Modify generated line endings to UNIX style
find . -name '*.[hc]' | xargs dos2unix
