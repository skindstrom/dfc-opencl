#!/bin/sh
git diff --name-only --diff-filter=d HEAD | grep '\.c\|\.h\|\.cpp\|\.hpp' | xargs clang-format -i -style=file &> /dev/null
