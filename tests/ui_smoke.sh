#!/bin/sh

set -u

if ! command -v xvfb-run >/dev/null 2>&1; then
  echo "ui smoke: skipped (xvfb-run not installed)"
  exit 0
fi

if ! xvfb-run -a -s '-screen 0 1280x720x24' xdpyinfo >/dev/null 2>&1; then
  echo "ui smoke: skipped (Xvfb cannot provide a display)"
  exit 0
fi

log=$(mktemp "${TMPDIR:-/tmp}/wmdpaint-ui.XXXXXX")
trap 'rm -f "$log"' EXIT HUP INT TERM

SDL_AUDIODRIVER=dummy timeout 6s xvfb-run -a \
  -s '-screen 0 1280x720x24' ./wmdpaint >"$log" 2>&1
rc=$?

# A healthy GUI remains open until the smoke-test timeout terminates it.
if [ "$rc" -ne 124 ]; then
  cat "$log"
  echo "ui smoke: application exited unexpectedly (status $rc)" >&2
  exit 1
fi

echo "ui smoke: SDL/Nuklear startup ok"
