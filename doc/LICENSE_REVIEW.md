# Licensing review

The original wmdpaint code is distributed under the MIT License; see
[`LICENSE`](LICENSE). Third-party components remain under their own licenses.

## GPL-family review

A repository-wide text scan found no GPL, LGPL, or AGPL license notice or
other declaration that code in this tree is licensed under a GPL-family
license. This is an inventory aid, not a legal conclusion or a source-history
audit.

## Items to consider before redistribution

- `assets/fonts/Code.ttf` and `assets/fonts/runescape_uf.ttf` identify
  themselves as Creative Commons Attribution-NoDerivatives 3.0. **Release
  consideration:** this is restrictive and is not the MIT License; verify that
  the intended distribution and embedding are allowed, preserve attribution,
  and do not distribute modified versions.
- `assets/fonts/Alkhemikal.ttf` and `assets/fonts/ari-w9500*.ttf` contain no
  recognizable embedded license string and have no accompanying license file.
  Confirm their provenance and redistribution/embedding rights before release.
- The `ModernDOS*.ttf` and `PixelOperator*.ttf` files identify themselves as
  CC0 1.0, but an accompanying copy of the license and provenance record should
  still be added. Do not assume the project's MIT License covers any font.
- `assets/wmdlogo.png`, `assets/icons.1fpirate`, `src/logo_data.h`, and
  `src/icons_data.h`: confirm authorship/provenance and record the applicable
  license. The generated headers should track the license of their source
  artwork.
- `src/nuklear_sdl_renderer.h`, `src/nuklear_sdl3_renderer.h`, and
  `src/nuklear_xlib.h`: these appear to come from the Nuklear project, but the
  individual files do not carry a complete license notice. Record their exact
  upstream revision and retain the applicable upstream license alongside them.
- `src/*.orig`, `src/*.bak`, `src/*.rej`, `src/config.h~`, and `src/old/`:
  verify that these historical/editor artifacts are intended for release and
  have the same provenance as the current project sources, or omit them from
  release archives.
- The checked-in `wmdpaint` executable should be reproducibly rebuilt from the
  reviewed sources for a release; a text scan cannot establish which sources
  or libraries were used to produce an existing binary.

## Identified permissive third-party code

- `src/nuklear.h`: dual MIT/public-domain notice is embedded in the file.
- `src/stb_image.h`, `src/stb_image_write.h`, and `src/stb_truetype.h`: dual
  MIT/public-domain notices are embedded in the files.

The build also links to SDL and the platform C/math libraries. Binary
distributions should include notices required by the exact library versions
used by the distributor.
