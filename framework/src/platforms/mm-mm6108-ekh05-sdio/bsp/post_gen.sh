#!/usr/bin/env sh

## Post processing script for CubeMX code generator

# Remove unused generated code
[ -d "Drivers/" ] && rm -r Drivers/

# Find Homebrew dos2unix when not on PATH (macOS)
command -v dos2unix >/dev/null || PATH="$PATH:/opt/homebrew/bin"

# Modify generated line endings to UNIX style
find . -name '*.[hc]' -o -name '*.ioc' | xargs dos2unix
