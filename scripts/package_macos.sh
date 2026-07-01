#!/usr/bin/env bash
#
# Package Crave as a macOS .app bundle (and a .dmg for distribution).
#
#   ./scripts/package_macos.sh
#
# Produces dist/Crave.app and dist/Crave.dmg. Run on macOS, hopefully :D
#
set -euo pipefail

APP_NAME="Crave"
BUNDLE_ID="io.github.bichanna.crave"
VERSION="0.1.0"
MIN_MACOS="11.0"

cd "$(dirname "$0")/.."

if [[ "$(uname)" != "Darwin" ]]; then
  echo "error: this script must run on macOS" >&2
  exit 1
fi

echo ">> release build"
meson setup build-release --buildtype=release --reconfigure >/dev/null 2>&1 ||
  meson setup build-release --buildtype=release
meson compile -C build-release

APP="dist/${APP_NAME}.app"
CONTENTS="${APP}/Contents"
echo ">> assembling ${APP}"
rm -rf "${APP}"
mkdir -p "${CONTENTS}/MacOS" "${CONTENTS}/Resources"

cp "build-release/crave" "${CONTENTS}/MacOS/${APP_NAME}"
chmod +x "${CONTENTS}/MacOS/${APP_NAME}"

echo ">> Info.plist"
cat >"${CONTENTS}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>             <string>${APP_NAME}</string>
  <key>CFBundleDisplayName</key>      <string>${APP_NAME}</string>
  <key>CFBundleIdentifier</key>       <string>${BUNDLE_ID}</string>
  <key>CFBundleVersion</key>          <string>${VERSION}</string>
  <key>CFBundleShortVersionString</key> <string>${VERSION}</string>
  <key>CFBundleExecutable</key>       <string>${APP_NAME}</string>
  <key>CFBundleIconFile</key>         <string>${APP_NAME}</string>
  <key>CFBundlePackageType</key>      <string>APPL</string>
  <key>CFBundleInfoDictionaryVersion</key> <string>6.0</string>
  <key>LSMinimumSystemVersion</key>   <string>${MIN_MACOS}</string>
  <key>NSHighResolutionCapable</key>  <true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.food-and-drink</string>
  <key>NSHumanReadableCopyright</key> <string>Crave</string>
</dict>
</plist>
PLIST

# build Crave.icns from packaging/icon.png
if [[ -f packaging/icon.png ]]; then
  echo ">> icon"
  ICONSET="$(mktemp -d)/${APP_NAME}.iconset"
  mkdir -p "${ICONSET}"
  sips -z 16 16 packaging/icon.png --out "${ICONSET}/icon_16x16.png" >/dev/null
  sips -z 32 32 packaging/icon.png --out "${ICONSET}/icon_16x16@2x.png" >/dev/null
  sips -z 32 32 packaging/icon.png --out "${ICONSET}/icon_32x32.png" >/dev/null
  sips -z 64 64 packaging/icon.png --out "${ICONSET}/icon_32x32@2x.png" >/dev/null
  sips -z 128 128 packaging/icon.png --out "${ICONSET}/icon_128x128.png" >/dev/null
  sips -z 256 256 packaging/icon.png --out "${ICONSET}/icon_128x128@2x.png" >/dev/null
  sips -z 256 256 packaging/icon.png --out "${ICONSET}/icon_256x256.png" >/dev/null
  sips -z 512 512 packaging/icon.png --out "${ICONSET}/icon_256x256@2x.png" >/dev/null
  sips -z 512 512 packaging/icon.png --out "${ICONSET}/icon_512x512.png" >/dev/null
  cp packaging/icon.png "${ICONSET}/icon_512x512@2x.png"
  iconutil -c icns "${ICONSET}" -o "${CONTENTS}/Resources/${APP_NAME}.icns"
else
  echo ">> (no packaging/icon.png, so app will use the default icon)"
fi

# ad-hoc code signature so the app launches without "damaged" errors
echo ">> ad-hoc code signing"
codesign --force --deep --sign - "${APP}"
codesign --verify --deep --strict "${APP}" && echo "   signature ok"

# DMG for handing the app to someone.
echo ">> dmg"
DMG="dist/${APP_NAME}.dmg"
rm -f "${DMG}"
STAGE="$(mktemp -d)"
cp -R "${APP}" "${STAGE}/"
ln -s /Applications "${STAGE}/Applications"
hdiutil create -volname "${APP_NAME}" -srcfolder "${STAGE}" -ov -format UDZO "${DMG}" >/dev/null
rm -rf "${STAGE}"

echo ""
echo "done:"
echo "  ${APP}"
echo "  ${DMG}"
