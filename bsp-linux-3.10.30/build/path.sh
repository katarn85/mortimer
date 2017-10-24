#!/bin/sh
echo "set toolchain path"
export TOP_DIR=$(pwd)/..
export TOOLCHAIN_DIR=$(pwd)/toolchain
export PATH=$TOOLCHAIN_DIR/opt/cross/bin:$PATH
build_dir=$(pwd)
alias bb='cd $build_dir'
alias tt='cd $TOP_DIR'

