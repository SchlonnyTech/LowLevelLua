#!/bin/bash

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

echo "Paste commit message, then press Ctrl+D on a new line:"
cat > "$tmp"

if [ ! -s "$tmp" ]; then
  echo "❌ Empty commit message, aborting." >&2
  exit 1
fi

echo
echo "──────── Message preview ────────"
cat "$tmp"
echo "──────────────────────────────────"
echo "Bytes: $(wc -c < "$tmp")   Lines: $(wc -l < "$tmp")"
echo

read -p "Commit and push? [y/N] " ans
case "$ans" in
  [yY]|[yY][eE][sS]) ;;
  *) echo "Aborted."; exit 0 ;;
esac

git add . \
  && git commit -F "$tmp" \
  && git pull https://github.com/SchlonnyTech/LowLevelLua.git main --rebase \
  && git push https://github.com/SchlonnyTech/LowLevelLua.git main
