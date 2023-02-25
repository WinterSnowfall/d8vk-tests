#!/usr/bin/env bash

set -e

shopt -s extglob

if [ -z "$1" ]; then
  echo "Usage: $0 <destdir> [--dev-build]"
  echo ""
  echo "Ex: $0 build --dev-build"
  exit 1
fi

D8VKTESTS_SRC_DIR=`dirname $(readlink -f $0)`
D8VKTESTS_BUILD_DIR=$(realpath "$1")"/d8vk-tests"

if [ -e "$D8VKTESTS_BUILD_DIR" ]; then
  echo "Clearing existing build directory $D8VKTESTS_BUILD_DIR"
  rm -rf "$D8VKTESTS_BUILD_DIR"
fi

shift 1

opt_devbuild=0

crossfile="build-win"

while [ $# -gt 0 ]; do
  case "$1" in
  "--dev-build")
    opt_devbuild=1
    ;;
  *)
    echo "Unrecognized option: $1" >&2
    exit 1
  esac
  shift
done

function build_arch {
  cd "$D8VKTESTS_SRC_DIR"

  opt_strip=
  if [ $opt_devbuild -eq 0 ]; then
    opt_strip=--strip
  fi

  meson --cross-file "$D8VKTESTS_SRC_DIR/$crossfile$1.txt" \
        --buildtype "release"                              \
        --prefix "$D8VKTESTS_BUILD_DIR"                    \
		    $opt_strip                                         \
        --bindir "x$1"                                     \
        --libdir "x$1"                                     \
        "$D8VKTESTS_BUILD_DIR/build.$1"

  echo "*" > "$D8VKTESTS_BUILD_DIR/../.gitignore"

  cd "$D8VKTESTS_BUILD_DIR/build.$1"
  ninja install

  if [ $opt_devbuild -eq 0 ]; then
    rm -R "$D8VKTESTS_BUILD_DIR/build.$1"
  fi
}

build_arch 32

