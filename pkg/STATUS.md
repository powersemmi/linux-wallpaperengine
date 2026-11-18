# Status

## Done

- Fork `powersemmi/linux-wallpaperengine`, default branch `moloko` on top of `upstream/main`.
- Background layer by default. Upstream already has `--layer`; the branch only changes its default
  from `bottom` to `background`, so no patch to the Wayland viewport is carried.
- CEF's `-march=x86-64` and `-O2` no longer override the toolchain flags.
- The bundled libraries are linked statically and stay out of the install: the package holds only
  the program and CEF.
- Package `linux-wallpaperengine-moloko`: GCC, `-march=native`, `-O3`, LTO; flags `_lto`, `_o3`.
- paru finds the package with `Path = pkg` (checked with a scratch paru.conf).

## Decisions

- GCC, not clang: upstream and the AUR packages build with GCC, CEF's flag set carries GCC-only
  warning options under `-Werror`, and `clang` in `PATH` is a cache shim.
- LTO through CMake (`CMAKE_INTERPROCEDURAL_OPTIMIZATION`), makepkg's own LTO is off
  (`options=(!lto)`). The link uses ld.bfd: lld cannot link GCC LTO objects.
- The package is not a `-git` package: paru rebuilds it on `paru -S`, not on every `-Syu`.

## Known limitations

- Every build downloads CEF (~350 MB) from the Spotify CDN.

## Next step

- The user adds the paru.conf entry and installs; then the niri layer-rule and autostart from
  `pkg/README.md`.
