#!/usr/bin/env bash

set -e

version="${1:-0.1.0-unknown}"
version="${version#v}"
base="firebolt-native-transport"
dist_name="firebolt-native-transport-$version"
dist_path="build/$dist_name"

mkdir -p "$dist_path"
rm -f build/$base-*.tar.gz

git checkout-index -a -f --prefix="$dist_path/"
sed -i -e '/# <VERSION_BLOCK>/,/# <\/VERSION_BLOCK>/cset(PROJECT_VERSION \"'"$version"'\")' "$dist_path/CMakeLists.txt"
tar -czf "build/$dist_name.tar.gz" -C "build" "$dist_name"

