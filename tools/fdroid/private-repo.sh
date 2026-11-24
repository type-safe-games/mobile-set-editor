#!/usr/bin/env bash
# Mobile Set Editor - publish F-Droid builds to the private repo on the tailnet (#93)
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tools/fdroid/private-repo.sh publish    sign the APKs `local.sh build` made (out/fdroid/data/unsigned)
#                                           with the release key, and update the repo's signed index
#   tools/fdroid/private-repo.sh serve      serve the repo on the tailnet (tailscale serve, HTTPS)
#   tools/fdroid/private-repo.sh info       the repo's URL and fingerprint, for Droid-ify
#
# The repo lives in $MSE_FDROID_REPO (default ~/mse-fdroid: config.yml, metadata/, repo/), the
# keystore in $MSE_RELEASE_KEYS (default ~/.config/mse-release: release.p12, keystore-password).
# Neither is in git. See tools/fdroid/README.md.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
APP=io.github.type_safe_games.mobileseteditor
REPO=${MSE_FDROID_REPO:-$HOME/mse-fdroid}
KEYS=${MSE_RELEASE_KEYS:-$HOME/.config/mse-release}
W=${FDROID_WORK:-$ROOT/out/fdroid}
IMG=${FDROID_IMAGE:-registry.gitlab.com/fdroid/fdroidserver:buildserver}

die() { echo "fdroid/private-repo.sh: $*" >&2; exit 1; }

fdroid() {
  [[ -f $KEYS/release.p12 && -f $KEYS/keystore-password ]] || die "no keystore in $KEYS"
  [[ -f $REPO/config.yml ]] || die "no repo in $REPO (config.yml)"
  docker run --rm -u vagrant -e FDROID_KEYSTORE_PASS="$(cat "$KEYS/keystore-password")" \
    -v "$W/fdroidserver:/home/vagrant/fdroidserver:ro" -v "$KEYS:/keystore:ro" -v "$REPO:/repo" \
    --entrypoint bash "$IMG" -c \
    ". /etc/profile; export PATH=/home/vagrant/fdroidserver:\$PATH PYTHONPATH=/home/vagrant/fdroidserver; cd /repo; $*"
}

publish() {
  compgen -G "$W/data/unsigned/${APP}_*.apk" >/dev/null || die "no unsigned APKs: run tools/fdroid/local.sh build first"
  # only versions the repo doesn't have: a published APK is never replaced (fdroid refuses anyway)
  local apk new=()
  for apk in "$W"/data/unsigned/"${APP}"_*.apk; do
    if [[ -e $REPO/repo/$(basename "$apk") ]]; then echo "fdroid/private-repo.sh: $(basename "$apk") is already published"
    else cp "$apk" "$REPO/unsigned/"; new+=("$(basename "$apk")"); fi
  done
  # the recipe, and the store listing in fdroiddata's layout (metadata/<app>/<locale>/)
  cp "$ROOT/tools/fdroid/$APP.yml" "$REPO/metadata/"
  rm -rf "$REPO/metadata/$APP"
  mkdir -p "$REPO/metadata/$APP"
  cp -r "$ROOT/fastlane/metadata/android/." "$REPO/metadata/$APP/"
  # the repo's own icon (config.yml's repo_icon), the app's store icon
  mkdir -p "$REPO/repo/icons"
  cp "$ROOT/fastlane/metadata/android/en-US/images/icon.png" "$REPO/repo/icons/icon.png"
  if [[ ${#new[@]} -gt 0 ]]; then fdroid "fdroid publish -v $APP && fdroid update --verbose"
  else fdroid "fdroid update --verbose"; fi
}

serve() {
  tailscale serve --bg --set-path /fdroid/repo "$REPO/repo"
  tailscale serve status
}

info() {
  local host fp
  host=$(tailscale status --self --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["Self"]["DNSName"].rstrip("."))')
  fp=$(keytool -exportcert -keystore "$KEYS/release.p12" -storepass "$(cat "$KEYS/keystore-password")" -alias repo 2>/dev/null | sha256sum | cut -c1-64)
  echo "URL:         https://$host/fdroid/repo"
  echo "Fingerprint: ${fp^^}"
  echo "Droid-ify:   https://$host/fdroid/repo?fingerprint=${fp^^}"
}

case ${1:-} in
  publish) publish ;;
  serve) serve ;;
  info) info ;;
  *) die "usage: $0 publish|serve|info" ;;
esac
