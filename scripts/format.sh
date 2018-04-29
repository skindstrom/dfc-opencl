#!/bin/sh
source_files=$(find src example tests benchmark | grep '\.c\|\.h\|\.cpp\|\.hpp\|\.cl')

while read -r source; do
  clang-format -i -style=file $source
done <<< $source_files