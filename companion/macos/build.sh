#!/bin/zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h:h}
build_dir="$script_dir/build"
app_dir="$repo_root/dist/PocketPDA Companion.app"
zip_path="$repo_root/dist/PocketPDA-Companion-macOS-v0.3.0.zip"

mkdir -p "$build_dir" "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources"
sdk_path="/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk"
for architecture in arm64 x86_64; do
  swiftc -parse-as-library \
    -sdk "$sdk_path" -module-cache-path "$build_dir/ModuleCache-$architecture" \
    -target "$architecture-apple-macosx14.0" \
    -framework SwiftUI -framework AppKit -framework EventKit \
    "$script_dir/Sources/PocketPDACompanion.swift" \
    "$script_dir/Sources/CompanionBackup.swift" \
    "$script_dir/Sources/CompanionVault.swift" \
    "$script_dir/Sources/CompanionCalendar.swift" \
    "$script_dir/Sources/CompanionToolsView.swift" \
    "$script_dir/Sources/CompanionFirmwareView.swift" \
    "$script_dir/Sources/CompanionCalendarToolsView.swift" \
    "$script_dir/Sources/PagerData.swift" \
    "$script_dir/Sources/PagerDesktopModel.swift" \
    "$script_dir/Sources/PagerDesktopView.swift" \
    "$script_dir/Sources/DesktopCalendarView.swift" \
    "$script_dir/Sources/DesktopCalendarLayout.swift" \
    "$script_dir/Sources/DesktopLayout.swift" \
    "$script_dir/Sources/DesktopSettingsView.swift" \
    "$script_dir/Sources/PagerDemo.swift" \
    -o "$build_dir/PocketPDACompanion-$architecture"
done
lipo -create \
  "$build_dir/PocketPDACompanion-arm64" \
  "$build_dir/PocketPDACompanion-x86_64" \
  -output "$app_dir/Contents/MacOS/PocketPDACompanion"
cp "$script_dir/Info.plist" "$app_dir/Contents/Info.plist"
cp "$repo_root/tools/pocketpda-editor.html" "$app_dir/Contents/Resources/pocketpda-editor.html"
cp "$repo_root/tools/flash_update.py" "$app_dir/Contents/Resources/flash_update.py"
if [[ -f "$repo_root/.pio/build/tlora_pager/firmware.bin" ]]; then
  cp "$repo_root/.pio/build/tlora_pager/firmware.bin" "$app_dir/Contents/Resources/PocketPDA-tlora_pager.bin"
fi
cp "$script_dir/Assets/AppIcon.icns" "$app_dir/Contents/Resources/AppIcon.icns"
xattr -cr "$app_dir"
xattr -d com.apple.FinderInfo "$app_dir" 2>/dev/null || true
xattr -d 'com.apple.fileprovider.fpfs#P' "$app_dir" 2>/dev/null || true
codesign --force --deep --sign - "$app_dir"
rm -f "$zip_path"
ditto -c -k --norsrc --keepParent "$app_dir" "$zip_path"
# File Provider can reapply Finder metadata while ditto reads an app on Desktop.
# Remove it again; these metadata attributes are not part of the signature.
xattr -d com.apple.FinderInfo "$app_dir" 2>/dev/null || true
xattr -d 'com.apple.fileprovider.fpfs#P' "$app_dir" 2>/dev/null || true
echo "$app_dir"
echo "$zip_path"
