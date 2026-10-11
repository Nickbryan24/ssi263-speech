#!/bin/sh
# The app icon: drawn at 1024 (make_icon.swift), then scaled to the Mac's sizes (iOS takes the 1024 alone).
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SET="$HERE/app/Assets.xcassets/AppIcon.appiconset"
xcrun swift "$HERE/tools/make_icon.swift" "$SET/icon-1024.png"
for n in 16 32 64 128 256 512; do
    sips -z $n $n "$SET/icon-1024.png" --out "$SET/icon-$n.png" >/dev/null
done
