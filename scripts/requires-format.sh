#!/bin/sh
function is_valid_staged() {
  staged=$(git diff --name-only --diff-filter=d HEAD)
  source_files=$(echo $staged | grep '\.c\|\.h\|\.cpp\|\.hpp\|\.cl')
  while read -r source; do
    if require_format $source
    then
      echo "$source needs formatting"
    fi
  done <<< "$source_files"
}

function require_format() {
  replacements=$(clang-format -style=file -output-replacements-xml $1)

  # grep returns 0 on match
  echo $replacements | grep "<replacement " >/dev/null
}

out=$(is_valid_staged)
echo $out
if [ -z "$out" ]
then
  exit 0
else
  exit 1
fi
