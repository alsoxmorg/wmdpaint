#!/bin/sh
# rawpull.sh — pull latest wmdpaint files from tsorange5

set -e

# sanity check: make sure we're in the right directory
if [ ! -f ./Makefile ] || [ ! -d ./src ]; then
    echo "rawpull: run this from the wmdpaint project root" >&2
    exit 1
fi

REMOTE="tsorange5:/home/tcooper/devel/wmdpaint/"

echo "rawpull: syncing from $REMOTE ..."
rsync -av --progress "$REMOTE" ./

echo "rawpull: done."
