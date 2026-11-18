# linux-wallpaperengine (moloko fork): rules for agents

This is a fork of Almamu/linux-wallpaperengine, packaged for one machine (Ryzen 9 7900X, niri on
Wayland). The user-facing description is `pkg/README.md` (Russian). Progress, decisions and known
limitations live in `pkg/STATUS.md`; update it whenever the package changes state.

## Branches

- `main` mirrors `upstream/main` (https://github.com/Almamu/linux-wallpaperengine) and never gets
  commits of its own.
- `moloko` is the default branch on GitHub. It is `upstream/main` plus a short series of real
  commits: the background-layer default, two `CMakeLists.txt` fixes (CEF flags, bundled
  libraries out of the install), a `[[maybe_unused]]` on `ScriptEngine::m_mediaSource` that
  clang needs under CEF's `-Werror`, the package in `pkg/`, and these notes. Keep the series short
  and rebase-friendly; code changes go in their own commits, separate from packaging.

## Syncing with upstream

```sh
git fetch upstream
git switch main && git merge --ff-only upstream/main && git push origin main
git switch moloko && git rebase upstream/main
# build and smoke-test (below), then:
git push --force-with-lease origin moloko
```

If upstream changes the `--layer` option, the CEF flag handling or the `add_subdirectory` calls of
`src/External`, re-check the code commits against it; drop a commit that upstream made redundant.

## The package

- `pkg/PKGBUILD` builds `linux-wallpaperengine-moloko` from the fork's `moloko` branch over
  git+https: a local build tests what is pushed, not the working tree.
- After any PKGBUILD change regenerate `pkg/.SRCINFO` with `makepkg --printsrcinfo > .SRCINFO`
  (inside `pkg/`) and commit both.
- Build flags follow the linux-moloko kernel package: `_name=value` from the environment, checked in
  `_check_flags` at the start of `prepare()`. A new flag is a default line at the top, a `_flag`
  call, its use in `build()`, and a row in the README table.
- Compiler: `_cc=clang` (default) or `_cc=gcc`, always by path (`/usr/bin/clang`,
  `/usr/bin/clang++`, `/usr/bin/gcc`, `/usr/bin/g++`): `clang` in the user's `PATH` is a
  compiler-cache shim. Clang links with lld and archives with `llvm-ar`/`llvm-ranlib`. GCC uses
  `gcc-ar`/`gcc-ranlib` and ld.bfd, because lld cannot link GCC LTO objects; the PKGBUILD drops the
  `-fuse-ld=lld` of the user's makepkg.conf in that case.
- LTO: `_lto=full|thin|no`, default `full`. The PKGBUILD puts `-flto=<mode>` (GCC: `-flto=auto`)
  into CFLAGS, CXXFLAGS and LDFLAGS itself instead of using CMake's IPO switch, which would pick
  ThinLTO for clang. CEF's macros test each warning option with `check_cxx_compiler_flag`, so the
  GCC-only `-Wno-*` options drop out under clang and `-Werror` stays on.
- `-march` and `-O` come from `CMAKE_<LANG>_FLAGS(_RELEASE)`. CEF's macros append their own
  `-march=x86-64` and `-O2` after them; `CMakeLists.txt` removes those two items. Check the
  effective flags in `build/compile_commands.json` after a rebase: every target must show the
  chosen compiler, `-march=native`, `-O3` and `-flto=full`.
- The bundled libraries in `src/External` are added with `EXCLUDE_FROM_ALL` and kissfft is static:
  `/opt/linux-wallpaperengine` holds only the program and CEF. After a rebase, list the package
  contents (`tar -tf *.pkg.tar.zst`) and check that no `include/`, `bin/` or tool binaries crept in.
- The runtime `depends` were taken from the `NEEDED` entries of the program, its library and the
  CEF libraries; recheck them when upstream adds a dependency.
- paru discovers the package through `Path = pkg` in the user's `/etc/paru.conf`.

## Building and verifying

- `cd pkg && makepkg -Cf` (no `-s`/`-i`: they need sudo). The user's makepkg.conf puts the build
  in `/tmp/makepkg/linux-wallpaperengine-moloko`. A full build takes minutes and downloads CEF
  (~350 MB): run it in the background and do not poll it.
- Smoke test without installing, in the user's niri session:
  run `/tmp/makepkg/linux-wallpaperengine-moloko/pkg/linux-wallpaperengine-moloko/opt/linux-wallpaperengine/linux-wallpaperengine --silent --screen-root DP-3 2305352822`
  under `timeout -s INT 12`. While it runs, `niri msg layers` must list the
  `linux-wallpaperengine` surface under the Background layer of DP-3, and `grim -o DP-3` shows
  the frame. The log must end with `Stopping`, without a crash.
- Never touch `/etc/paru.conf`, the niri config or the Noctalia config: they belong to the user.
