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
- Workspace parallax (`--workspace-parallax niri|stdin` and related flags): the wallpaper follows
  niri's active workspace with a spring that copies niri's workspace-switch spring. Verified on
  DP-3: the image mode moves the image 58 px for 4 % of 1440 px; the layers mode on 2305352822
  moves its layers 28, 36 and 52 px by depth; idle CPU unchanged (54 vs 50 ticks over 5 s).
  The W Engine Noctalia plugin passes the flags once the installed engine lists them in `--help`.
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

- Workspace parallax lives in the engine, not in the shell: niri's event stream is read on a thread
  of the engine, and the spring is stepped once per frame on the render thread (an atomic load per
  frame; a lock only when a workspace changes). The shell only passes flags. niri does not stream
  its animation progress, so the engine animates on its own with niri's spring parameters.
- Workspace position maps to [0, 1] across the output's workspaces (the Android wallpaper-offset
  convention), so the total travel stays the same for any number of workspaces.
- `--disable-parallax` still turns off only the mouse parallax; workspace parallax in the layers
  mode works without it.

## Known limitations

- Workspace parallax does not read `~/.config/niri/config.kdl`: a custom workspace-switch spring
  has to be passed with `--workspace-parallax-stiffness` and `--workspace-parallax-damping-ratio`.
  niri's animation slowdown and touchpad workspace gestures are not followed; a gesture moves the
  wallpaper when it ends.
- When niri adds or removes the trailing empty workspace, the wallpaper glides to its new place
  without a switch.
- In the layers mode a layer with depth + amount above 1 can show its edge.
- Every build downloads CEF (~350 MB) from the Spotify CDN.

## Next step

- The user adds the paru.conf entry and installs; then the niri layer-rule and autostart from
  `pkg/README.md`.
