#!/bin/sh
set -e
root="$1"
if [ ! -d "$root" ]; then
  echo "usage: gen-qmldir.sh <shell directory>" >&2
  exit 2
fi
root="${root%/}"
find "$root" -type d -not -path '*/.git' -not -path '*/.git/*' | while read -r dir; do
  rel="${dir#"$root"}"
  rel="${rel#/}"
  if [ -z "$rel" ]; then
    continue
  fi
  files=$(find "$dir" -maxdepth 1 -type f -name '[A-Z]*.qml' | sort)
  if [ -z "$files" ]; then
    rm -f "$dir/qmldir"
    continue
  fi
  mod="qs.$(printf '%s' "$rel" | tr '/' '.')"
  out="$dir/qmldir"
  printf 'module %s\n' "$mod" > "$out"
  for f in $files; do
    name=$(basename "$f" .qml)
    if grep -q '^pragma Singleton' "$f"; then
      printf 'singleton %s 1.0 %s.qml\n' "$name" "$name" >> "$out"
    else
      printf '%s 1.0 %s.qml\n' "$name" "$name" >> "$out"
    fi
  done
done
