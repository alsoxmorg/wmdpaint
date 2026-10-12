# wmdpaint Qt experiment

This directory contains an experimental Qt 6 Widgets frontend. It is separate
from the SDL/Nuklear application and builds a binary named `wmdpaint-qt`.

Implemented so far:

- native 16-bit RGBA farbfeld loading and saving
- conventional image import through Qt image plugins
- pencil and eraser tools
- color selection, zooming, scrolling, and drag-and-drop opening
- unsaved-change tracking and prompts

Build and run from the repository root (a shadow build keeps generated files
out of the source tree):

```sh
mkdir -p build/qt
cd build/qt
qmake6 ../../src/qt/wmdpaint-qt.pro
make
./wmdpaint-qt [image.ff]
```

This is intentionally an evaluation frontend, not yet a feature-complete port.
The SDL/Nuklear build in the repository root is unchanged.
