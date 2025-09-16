#!/usr/bin/env bash

set -e

bdir="build"
do_install=false

while [[ ! -z $1 ]]; do
  case $1 in
  --clean ) rm -rf $bdir;;
  --) shift; break;;
  -i | --install) do_install=true;;
  *) SYSROOT_PATH="$1"; shift;;
  esac; shift
done

[[ ! -z $SYSROOT_PATH ]] || { echo "SYSROOT_PATH not set" >/dev/stderr; exit 1; }
[[ -e $SYSROOT_PATH ]] || { echo "SYSROOT_PATH not exist ($SYSROOT_PATH)" >/dev/stderr; exit 1; }

[[ -e $bdir ]] || cmake -B $bdir -DSYSROOT_PATH=$SYSROOT_PATH "$@"
cmake --build $bdir
if $do_install; then
  cmake --install $bdir
fi

