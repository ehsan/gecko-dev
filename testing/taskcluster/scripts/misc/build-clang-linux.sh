#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

# Repository info
: CROSSTOOL_NG_REPOSITORY    ${CROSSTOOL_NG_REPOSITORY:=https://github.com/diorcety/crosstool-ng}
: CROSSTOOL_NG_REV           ${CROSSTOOL_NG_REV:=master}

# Fetch clang from tooltool
cd $HOME_DIR
wget -O tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
chmod +x tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

# Ninja
git clone git://github.com/martine/ninja.git
cd ninja
git checkout release
./configure.py --bootstrap
cp ninja /usr/local/bin/ninja
# Old versions of Cmake can only find ninja in this location!
cp ninja /usr/local/bin/ninja-build
cd ..
rm -rf ninja

cd src

# gets a bit too verbose here
set +x

$HOME_DIR/tooltool.py -m browser/config/tooltool-manifests/linux64/releng.manifest fetch
cd build/unix/build-clang
./build-clang.py -c clang-static-analysis-linux64-centos6.json
cd -

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp build/unix/build-clang/clang.tar.* $UPLOAD_DIR
