#!/usr/bin/env bash
# Mobile Set Editor - run F-Droid's checks and build of the recipe locally (#78)
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tools/fdroid/local.sh setup [<commit>]   fetch fdroidserver, fdroiddata's config, gradlew-fdroid
#                                            and the buildserver image; clone this repository with a
#                                            test tag v<appVersion> on <commit> (default HEAD)
#   tools/fdroid/local.sh lint               fdroid lint, and rewritemeta (which must change nothing)
#   tools/fdroid/local.sh scan               fdroid scanner on every build (fetches the source and
#                                            its submodules, like the buildserver)
#   tools/fdroid/local.sh build [<vc>...]    fdroid build --on-server of each versionCode (default:
#                                            every build in the recipe), each in a fresh container
#                                            whose only network is an allowlist proxy
#   tools/fdroid/local.sh clean              remove the proxy container and network
#
# Everything happens in out/fdroid (FDROID_WORK overrides it), in F-Droid's buildserver image
# (FDROID_IMAGE overrides it). Needs docker and git. See tools/fdroid/README.md.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
APP=io.github.type_safe_games.mobileseteditor
RECIPE=$ROOT/tools/fdroid/$APP.yml
W=${FDROID_WORK:-$ROOT/out/fdroid}
IMG=${FDROID_IMAGE:-registry.gitlab.com/fdroid/fdroidserver:buildserver}
NET=mse-fdroid-internal
PROXY=mse-fdroid-proxy

die() { echo "fdroid/local.sh: $*" >&2; exit 1; }
# fdroid in the buildserver image, with fdroidserver from $W, on the fdroiddata-like directory $1
fdroid_env='. /etc/profile; export PATH=/home/vagrant/fdroidserver:$PATH PYTHONPATH=/home/vagrant/fdroidserver PYTHONUNBUFFERED=true; cd /build'
mounts() {
  echo "-v $W/fdroidserver:/home/vagrant/fdroidserver:ro -v $1:/build -v $W/source-repo:/srv/source-repo:ro"
}
# the recipe's versionCodes
version_codes() { sed -nE 's/^ +versionCode: ([0-9]+)$/\1/p' "$RECIPE"; }

setup() {
  local commit=${1:-HEAD} version
  mkdir -p "$W"
  for repo in fdroidserver fdroiddata gradlew-fdroid; do
    if [[ -d $W/$repo ]]; then git -C "$W/$repo" pull -q --ff-only
    else git clone -q --depth 1 "https://gitlab.com/fdroid/$repo.git" "$W/$repo"; fi
  done
  docker pull -q "$IMG"
  # the source F-Droid builds: this repository, with the release tag the recipe names (only in
  # this clone: never push it)
  version=$(sed -nE 's/^val appVersion = "(.+)"$/\1/p' "$ROOT/android/app/build.gradle.kts")
  rm -rf "$W/source-repo"
  git clone -q --bare "$ROOT" "$W/source-repo"
  git --git-dir="$W/source-repo" tag -f "v$version" "$(git -C "$ROOT" rev-parse "$commit")" >/dev/null
  echo "fdroid/local.sh: source-repo has v$version at $(git -C "$ROOT" rev-parse --short "$commit")"
  # fdroiddata-like directories: lint/ with the recipe as it is, data/ with Repo: the local clone
  for d in lint data; do
    rm -rf "$W/$d/metadata" "$W/$d/config"
    mkdir -p "$W/$d/metadata"
    cp -r "$W/fdroiddata/config" "$W/$d/"
    echo 'repo_url: "https://example.invalid/repo"' > "$W/$d/config.yml"
  done
  sed 's|^Repo: .*|Repo: /srv/source-repo|' "$RECIPE" > "$W/data/metadata/$APP.yml"
}

lint() {
  cp "$RECIPE" "$W/lint/metadata/"
  docker run --rm -u vagrant $(mounts "$W/lint") --entrypoint bash "$IMG" -c \
    "$fdroid_env; fdroid lint -v $APP && fdroid rewritemeta $APP"
  if cmp -s "$RECIPE" "$W/lint/metadata/$APP.yml"; then echo "fdroid/local.sh: rewritemeta changed nothing"
  else diff -u "$RECIPE" "$W/lint/metadata/$APP.yml"; die "rewritemeta reformats the recipe: take its version"; fi
}

scan() {
  local vcs=() vc
  for vc in $(version_codes); do vcs+=("$APP:$vc"); done
  docker run --rm -u vagrant $(mounts "$W/data") --entrypoint bash "$IMG" -c "$fdroid_env; fdroid scanner -v ${vcs[*]}"
}

# a proxy that lets the build reach only what it may download, and logs every connection
start_proxy() {
  docker network inspect "$NET" >/dev/null 2>&1 || docker network create --internal "$NET" >/dev/null
  if ! docker container inspect "$PROXY" >/dev/null 2>&1; then
    docker run -d --name "$PROXY" -v "$ROOT/tools/fdroid/proxy:/cfg:ro" alpine:3 \
      sh -c 'apk add -q tinyproxy && exec tinyproxy -d -c /cfg/tinyproxy.conf' >/dev/null
    docker network connect "$NET" "$PROXY"
  fi
}

# runs as root in the build container: the network goes through the proxy (apt, sdkmanager,
# gradlew-fdroid and Gradle), then fdroid build runs as vagrant, as on F-Droid's buildserver
build_script='
set -e
. /etc/profile
echo "Acquire::http::Proxy \"$PROXY\"; Acquire::https::Proxy \"$PROXY\";" > /etc/apt/apt.conf.d/90proxy
export http_proxy=$PROXY https_proxy=$PROXY HTTP_PROXY=$PROXY HTTPS_PROXY=$PROXY
host=${PROXY#http://}; port=${host##*:}; host=${host%:*}
jvm_proxy="-Dhttp.proxyHost=$host -Dhttp.proxyPort=$port -Dhttps.proxyHost=$host -Dhttps.proxyPort=$port"
cd /build
mkdir -p build logs repo tmp unsigned /home/vagrant/.android
chown -R vagrant /home/vagrant/.android build logs repo tmp unsigned
# (sudo drops JAVA_TOOL_OPTIONS even with --preserve-env, so env sets it after sudo)
exec sudo --preserve-env --user vagrant env PATH=/home/vagrant/fdroidserver:$PATH \
  PYTHONPATH=/home/vagrant/fdroidserver PYTHONUNBUFFERED=true HOME=/home/vagrant \
  JAVA_TOOL_OPTIONS="$jvm_proxy" \
  fdroid build --verbose --on-server --no-tarball --no-refresh "$APP:$VC"
'

build() {
  local vcs=("$@") vc
  [[ ${#vcs[@]} -gt 0 ]] || mapfile -t vcs < <(version_codes)
  [[ -d $W/data/build/$APP ]] || die "no source in $W/data/build/$APP: run scan first"
  start_proxy
  for vc in "${vcs[@]}"; do
    echo "fdroid/local.sh: building $APP:$vc (log: $W/build-$vc.log)"
    docker run --rm --network "$NET" -e PROXY="http://$PROXY:8888" -e APP="$APP" -e VC="$vc" \
      $(mounts "$W/data") -v "$W/gradlew-fdroid:/home/vagrant/gradlew-fdroid:ro" \
      --entrypoint bash "$IMG" -c "$build_script" > "$W/build-$vc.log" 2>&1 ||
      { tail -40 "$W/build-$vc.log" >&2; die "the build of $vc failed"; }
  done
  echo "fdroid/local.sh: hosts the builds connected to (and were refused):"
  docker logs "$PROXY" 2>&1 | sed -nE 's/.*CONNECT ([^ :]+):.*/\1/p; s/.*refused on filtered domain "([^"]+)".*/\1 REFUSED/p' | sort | uniq -c
  ls -l "$W/data/unsigned/"
}

clean() {
  docker rm -f "$PROXY" >/dev/null 2>&1 || true
  docker network rm "$NET" >/dev/null 2>&1 || true
}

cmd=${1:-}; shift || true
case $cmd in
  setup|lint|scan|build|clean) "$cmd" "$@" ;;
  *) die "usage: $0 setup [<commit>] | lint | scan | build [<versionCode>...] | clean" ;;
esac
