#!/bin/zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h:h}
build_dir="$script_dir/build"
app_dir="$repo_root/dist/PocketPDA Companion.app"
zip_path="$repo_root/dist/PocketPDA-Companion-macOS-v0.1.1.zip"

mkdir -p "$build_dir" "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources"
sdk_path="/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk"
for architecture in arm64 x86_64; do
  swiftc -parse-as-library \
    -sdk "$sdk_path" -module-cache-path "$build_dir/ModuleCache-$architecture" \
    -target "$architecture-apple-macosx14.0" \
    -framework SwiftUI -framework AppKit -framework EventKit \
    "$script_dir/Sources/PocketPDACompanion.swift" \
    -o "$build_dir/PocketPDACompanion-$architecture"
done
lipo -create \
  "$build_dir/PocketPDACompanion-arm64" \
  "$build_dir/PocketPDACompanion-x86_64" \
  -output "$app_dir/Contents/MacOS/PocketPDACompanion"
cp "$script_dir/Info.plist" "$app_dir/Contents/Info.plist"
cp "$repo_root/tools/pocketpda-editor.html" "$app_dir/Contents/Resources/pocketpda-editor.html"
xattr -cr "$app_dir"
xattr -d com.apple.FinderInfo "$app_dir" 2>/dev/null || true
xattr -d 'com.apple.fileprovider.fpfs#P' "$app_dir" 2>/dev/null || true
codesign --force --deep --sign - "$app_dir"
rm -f "$zip_path"
ditto -c -k --norsrc --keepParent "$app_dir" "$zip_path"
# File Provider can reapply Finder metadata while ditto reads an app on Desktop.
# Remove it and refresh the local signature so both the app and ZIP verify.
xattr -d com.apple.FinderInfo "$app_dir" 2>/dev/null || true
xattr -d 'com.apple.fileprovider.fpfs#P' "$app_dir" 2>/dev/null || true
codesign --force --deep --sign - "$app_dir"
echo "$app_dir"
echo "$zip_path"
