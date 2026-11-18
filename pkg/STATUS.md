# Status

## Done

- Fork `powersemmi/linux-wallpaperengine`, default branch `moloko` on top of `upstream/main`.
- Background layer by default. Upstream already has `--layer`; the branch only changes its default
  from `bottom` to `background`, so no patch to the Wayland viewport is carried.
- CEF's `-march=x86-64` and `-O2` no longer override the toolchain flags.
- The bundled libraries are linked statically and stay out of the install: the package holds only
  the program and CEF.
- Package `linux-wallpaperengine-moloko`: clang + lld, `-march=native`, `-O3`, full LTO; flags
  `_lto=full|thin|no`, `_o3`, `_cc=clang|gcc`.
- paru finds the package with `Path = pkg` (checked with a scratch paru.conf).

## Decisions

- Clang by default, like the linux-moloko kernel (user's decision). GCC stays as `_cc=gcc`.
- Full LTO by default, not ThinLTO (user's decision). The flags are set by the PKGBUILD, not by
  CMake's IPO switch, which would choose ThinLTO for clang. makepkg's own LTO is off
  (`options=(!lto)`).
- CEF's `-Werror` stays on under clang: CEF checks each warning option before adding it.
  The only clang-specific failure was `-Wunused-private-field` on `ScriptEngine::m_mediaSource`;
  the field is marked `[[maybe_unused]]` in its own commit. No warning is suppressed globally.
- The package is not a `-git` package: paru rebuilds it on `paru -S`, not on every `-Syu`.

## Known limitations

- Every build downloads CEF (~350 MB) from the Spotify CDN.

## Next step

- The user adds the paru.conf entry and installs; then the niri layer-rule and autostart from
  `pkg/README.md`.
