#!/bin/sh

dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

git -C $dir submodule init && git -C $dir submodule update
git -C $dir submodule foreach 'git fetch origin; git checkout $(git rev-parse --abbrev-ref HEAD); git reset --hard origin/$(git rev-parse --abbrev-ref HEAD); git submodule update --recursive; git clean -dfx'

(cd $dir/modules/vzw-nidd-sdk && ./init.sh)

(cd $dir/modules/vzw-nidd-front-end && bundle && bun install)

(cd $dir && make clean && make)
