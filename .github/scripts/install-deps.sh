#!/usr/bin/env bash
# vim: ts=4

set -eu

die() {
    echo "$@" >/dev/stderr
    exit 1
}

cleanBefore=false
while [[ ! -z ${1:-} ]]; do
    case $1 in
    --clean) cleanBefore=true;;
    *) break;;
    esac; shift
done

SYSROOT_PATH="${1:-}"

[[ ! -z "${SYSROOT_PATH:-}" ]] || die "SYSROOT_PATH is not set"
[[ -e .deps ]] || die "dependency file not found"
SYSROOT_PATH="$(realpath "$SYSROOT_PATH")"

echo "installation dir: $SYSROOT_PATH"

cat .deps | while read name version url; do
    case $name in
    '#'* | '') continue;;
    esac

    dir="$name-$version"
    $cleanBefore && rm -rf $dir

    if [[ ! -e $dir ]]; then
        echo "downloading: $name@$version"
        case $name in
        nlohmann-json)
            git clone --depth 1 --branch "v$version" "$url" "$dir";;
        googletest)
            curl -sL $url/releases/download/v$version/$name-$version.tar.gz | tar xzf -;;
        *)
            curl -sL $url/archive/refs/tags/$version.tar.gz | tar xzf -;;
        esac
    fi

    echo "building: $name@$version"
    rm -rf "build/$dir"
    cmake -B "build/$dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX:PATH="$SYSROOT_PATH/usr" \
        -DBUILD_SHARED_LIBS=ON \
        "$dir"

    echo "Installing: $name@$version"
    cmake --build "build/$dir" --target install
done

