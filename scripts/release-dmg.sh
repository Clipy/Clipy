#!/usr/bin/env bash

set -euo pipefail

PROJECT="Clipy.xcodeproj"
SCHEME="Clipy"
CONFIGURATION="${CONFIGURATION:-Release}"
APP_NAME="${APP_NAME:-Clipy}"
VOLUME_NAME="${VOLUME_NAME:-Clipy}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-"${REPO_ROOT}/.build/release"}"
DERIVED_DATA_PATH="${DERIVED_DATA_PATH:-"${REPO_ROOT}/.build/DerivedData"}"
SOURCE_PACKAGES_PATH="${SOURCE_PACKAGES_PATH:-"${REPO_ROOT}/.build/SourcePackages"}"
ARCHIVE_PATH="${ARCHIVE_PATH:-"${BUILD_ROOT}/${APP_NAME}.xcarchive"}"
EXPORT_PATH="${EXPORT_PATH:-"${BUILD_ROOT}/export"}"
DMG_ROOT="${DMG_ROOT:-"${BUILD_ROOT}/dmgroot"}"
DMG_PATH="${DMG_PATH:-"${BUILD_ROOT}/${APP_NAME}.dmg"}"
EXPORT_OPTIONS_PLIST="${EXPORT_OPTIONS_PLIST:-"${BUILD_ROOT}/ExportOptions.plist"}"

usage() {
  cat <<'EOF'
Missing required environment variables.

Set CERT to your full Developer ID Application identity and TEAM_ID to your Apple Developer Team ID.

Examples:
  security find-identity -v -p codesigning

  export TEAM_ID=ABCDE12345
  export CERT="Developer ID Application: Jane Doe (ABCDE12345)"
  scripts/release-dmg.sh

Optional notarization:
  xcrun notarytool store-credentials "clipy-notary" \
    --apple-id "you@example.com" \
    --team-id "$TEAM_ID" \
    --password "app-specific-password"

  export NOTARY_PROFILE=clipy-notary
  scripts/release-dmg.sh
EOF
}

require_env() {
  local missing=0

  if [[ -z "${TEAM_ID:-}" ]]; then
    echo "TEAM_ID is not set." >&2
    missing=1
  fi

  if [[ -z "${CERT:-}" ]]; then
    echo "CERT is not set." >&2
    missing=1
  fi

  if [[ "${missing}" -ne 0 ]]; then
    echo >&2
    usage >&2
    exit 2
  fi
}

verify_identity() {
  if ! security find-identity -v -p codesigning | grep -F "${CERT}" >/dev/null; then
    echo "Could not find a matching codesigning identity for CERT:" >&2
    echo "  ${CERT}" >&2
    echo >&2
    echo "Available identities:" >&2
    security find-identity -v -p codesigning >&2 || true
    exit 2
  fi
}

write_export_options() {
  mkdir -p "${BUILD_ROOT}"
  cat >"${EXPORT_OPTIONS_PLIST}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>method</key>
  <string>developer-id</string>
  <key>teamID</key>
  <string>${TEAM_ID}</string>
  <key>signingStyle</key>
  <string>manual</string>
  <key>signingCertificate</key>
  <string>${CERT}</string>
</dict>
</plist>
EOF
}

archive_app() {
  echo "Archiving ${APP_NAME} with ${CERT}"
  rm -rf "${ARCHIVE_PATH}"

  xcodebuild \
    -project "${PROJECT}" \
    -scheme "${SCHEME}" \
    -configuration "${CONFIGURATION}" \
    -destination "generic/platform=macOS" \
    -archivePath "${ARCHIVE_PATH}" \
    -derivedDataPath "${DERIVED_DATA_PATH}" \
    -clonedSourcePackagesDirPath "${SOURCE_PACKAGES_PATH}" \
    -disablePackageRepositoryCache \
    -skipPackagePluginValidation \
    -skipMacroValidation \
    DEVELOPMENT_TEAM="${TEAM_ID}" \
    CODE_SIGN_STYLE=Manual \
    CODE_SIGN_IDENTITY="${CERT}" \
    PROVISIONING_PROFILE_SPECIFIER= \
    ENABLE_HARDENED_RUNTIME=YES \
    OTHER_CODE_SIGN_FLAGS="--timestamp" \
    clean archive
}

export_app() {
  echo "Exporting signed app"
  rm -rf "${EXPORT_PATH}"

  xcodebuild \
    -exportArchive \
    -archivePath "${ARCHIVE_PATH}" \
    -exportPath "${EXPORT_PATH}" \
    -exportOptionsPlist "${EXPORT_OPTIONS_PLIST}" \
    -skipPackagePluginValidation \
    -skipMacroValidation
}

create_dmg() {
  local app_path="${EXPORT_PATH}/${APP_NAME}.app"

  if [[ ! -d "${app_path}" ]]; then
    echo "Expected exported app not found: ${app_path}" >&2
    exit 1
  fi

  echo "Verifying app signature"
  codesign --verify --deep --strict --verbose=2 "${app_path}"

  echo "Creating DMG"
  rm -rf "${DMG_ROOT}"
  rm -f "${DMG_PATH}"
  mkdir -p "${DMG_ROOT}"
  cp -R "${app_path}" "${DMG_ROOT}/"
  ln -s /Applications "${DMG_ROOT}/Applications"

  hdiutil create \
    -volname "${VOLUME_NAME}" \
    -srcfolder "${DMG_ROOT}" \
    -ov \
    -format UDZO \
    "${DMG_PATH}"

  echo "Signing DMG"
  codesign --force --timestamp --sign "${CERT}" "${DMG_PATH}"
}

notarize_if_configured() {
  if [[ -z "${NOTARY_PROFILE:-}" ]]; then
    echo
    echo "DMG created without notarization:"
    echo "  ${DMG_PATH}"
    echo
    echo "To notarize automatically next time, set NOTARY_PROFILE to a notarytool keychain profile."
    return
  fi

  echo "Submitting DMG for notarization with profile ${NOTARY_PROFILE}"
  xcrun notarytool submit "${DMG_PATH}" \
    --keychain-profile "${NOTARY_PROFILE}" \
    --wait

  echo "Stapling notarization ticket"
  xcrun stapler staple "${DMG_PATH}"
  xcrun stapler validate "${DMG_PATH}"
}

final_verify() {
  echo "Final verification"
  codesign --verify --verbose=2 "${DMG_PATH}"

  if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    spctl -a -vv --type open "${DMG_PATH}"
  else
    echo "Skipping Gatekeeper assessment because the DMG was not notarized."
  fi

  echo
  echo "Release DMG:"
  echo "  ${DMG_PATH}"
}

cd "${REPO_ROOT}"
require_env
verify_identity
write_export_options
archive_app
export_app
create_dmg
notarize_if_configured
final_verify
