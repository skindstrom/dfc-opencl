#!/bin/sh
find src example tests -name '*.c' -or -name '*.h' -or -name '*.cpp' -or -name '*.hpp' -or -name '*.cl' -exec clang-format -i -style=file {} +
