# Continuing the TagLib branch on Linux

Everything on this branch was written on a Mac with no mpv, no glib and no
TagLib, so **nothing here has been compiled, linked or run**. The only thing
actually verified is that both Makefiles parse and expand to the intended
command lines (`make -n`). Treat the whole branch as a first draft with a
plausible shape, not as working code.

The API details were checked against the TagLib source at tag `v2.3.1` rather
than guessed: function names, the `taglib_picture_from_complex_property`
implementation (it memsets the output struct, but only once it has confirmed
`properties` is non-NULL), the `taglib_c.pc` include path (`${includedir}/taglib`,
hence `#include <tag_c.h>` and not `<taglib/tag_c.h>`), and the CMake option
names.

## What changed

- `mpris.c` — `libavformat` include gone; `extract_embedded_art` /
  `try_get_embedded_art` rewritten against TagLib, with a no-op stub compiled
  when `HAVE_TAGLIB` is undefined so `get_art_url` is untouched.
- `Makefile` — TagLib autodetected via `pkg-config --atleast-version=2.0
  taglib_c`, overridable with `USE_TAGLIB=0/1`. New `release` and
  `release-check` targets for the static portable build, plus `test-taglib`.
- `mpris.map` — linker version script; exports only `mpv_open_cplugin`.
- `.github/workflows/build.yml` — the existing job loses `libavformat-dev`; a
  new `release` job builds static TagLib from source and produces the artifact.
- `test/embedded-art` — new, opt-in, generates its own fixture with `ffmpeg`.
- `README.md`, `.gitignore`, `test/Makefile` — follow-on updates.

## Verification order

Work through these roughly in order; each one is where I would expect the next
problem to surface.

### 1. Plain build without TagLib

```fish
make clean
make USE_TAGLIB=0
```

Should compile cleanly and produce a plugin with no embedded art support. This
is the path Ubuntu 24.04, Fedora 43 and openSUSE Leap will take, since they all
still ship TagLib 1.13.1. Watch for `-Wunused-parameter` on the stub — the
`(void) path;` should cover it, but `-Wextra -pedantic` is unforgiving.

### 2. Build against a system TagLib 2

Arch has 2.3.1, so this should just work:

```fish
make clean
make
```

Confirm it actually picked TagLib up rather than silently falling back:

```fish
make -n | grep -o taglib
```

Then check the plugin loads and cover art still appears:

```fish
mpv --load-scripts=no --script=./mpris.so some-album-track.flac
playerctl metadata mpris:artUrl | head -c 80
```

### 3. The new test

```fish
make test-taglib
```

This is the least trustworthy thing on the branch. Known risks:

- The `ffmpeg` fixture invocation is written from memory and may need adjusting.
  The intent is a 10s sine tone with a 64x64 red PNG as an attached picture.
  Check the file really has an APIC frame before blaming the plugin.
- `test/wrapper` fails any test that writes to stderr. `-loglevel error
  -nostdin` should keep ffmpeg quiet, but if it grumbles the test fails for a
  reason that has nothing to do with the code under test.
- The fixture lands in the `test/` directory; it is gitignored and removed by
  `make -C test clean`, so it should not trip the CI untracked-files checks.

It is deliberately kept out of the default `make test` so that TagLib-less
builds do not fail. That does mean it only runs where TagLib is present.

### 4. The static release build

Build TagLib static somewhere, then point the Makefile at it:

```fish
git clone --depth 1 --branch v2.3.1 --recurse-submodules \
  https://github.com/taglib/taglib /tmp/taglib-src
cmake -S /tmp/taglib-src -B /tmp/taglib-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=(pwd)/build/taglib \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DVISIBILITY_HIDDEN=ON \
  -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
  -DWITH_MOD=OFF -DWITH_SHORTEN=OFF
cmake --build /tmp/taglib-build --parallel
cmake --install /tmp/taglib-build
make release
```

`--recurse-submodules` is not optional: TagLib vendors `utfcpp` as a submodule.
`CMAKE_POSITION_INDEPENDENT_CODE=ON` is likewise mandatory, since a non-PIC
archive cannot be linked into a shared object, and the resulting linker error is
not one of ld's more forthcoming messages.

`make release` compiles with `$(CC)` and links with `$(CXX)`, deliberately: the
source is C99 so it cannot be fed to `g++`, but the link step needs the C++
driver for `-static-libstdc++` to have anything to act on.

Then confirm the result is actually portable:

```fish
ldd mpris.so
nm -D --defined-only mpris.so
```

Wanted: glib, gobject, gio, gmodule, libz, libc, libm, libdl and whatever glib
drags in transitively — nothing versioned beyond that, no `libtag`, no
`libstdc++`. Exactly one defined symbol, `mpv_open_cplugin`. `make
release-check` asserts both, but read the output yourself the first time; the
symbol assertion in particular is written blind and may need loosening.

Finally, and most importantly, **load the release binary in mpv**. If the
version script is wrong, mpv simply will not find the entry point, and the
failure mode is a plugin that silently does nothing.

### 5. Two copies of libstdc++

Worth a paranoid check once it runs at all: a statically linked libstdc++ inside
a shared object is safe only while its symbols stay hidden, otherwise it can
interpose against another library in mpv's process. Nothing C++ crosses the
plugin boundary (mpv's ABI is C, TagLib's bindings are C), so hidden visibility
plus the version script should settle it — but exercise cover art under a
desktop mpv build with the full plugin set loaded, not just a bare test run.

## Decisions worth revisiting

- **`ubuntu-22.04` for the release job.** Chosen for the lower glibc floor
  (2.35 rather than 2.39). GitHub will retire that image eventually, and a
  container such as `debian:bullseye` would give both a lower floor and a stable
  target. Left as a runner for now because it needs no extra machinery.
- **Format coverage.** TagLib covers more than FFmpeg did here, not less. MP4
  `covr` art worked before and still does (`fileref.cpp:192` accepts `.mp4`,
  `.m4v`, `.m4b`, `.m4p`, `.3g2`). Matroska cover attachments never worked
  before: `libavformat/matroskadec.c` sets `AV_DISPOSITION_ATTACHED_PIC`
  nowhere, so attachments arrived as `AVMEDIA_TYPE_ATTACHMENT` streams that the
  old code ignored. TagLib maps any `image/*` attachment to the `PICTURE`
  property (`matroskafile.cpp:169`), so `.mkv` and `.webm` art is new
  functionality. It is also the newest code in TagLib, landing in 2.2 in
  February 2026 with a crash fix on invalid seek heads in 2.3.1 in July 2026,
  so it deserves more suspicion than the audio paths.
- **MIME sniffing.** The old code derived the MIME type from the FFmpeg codec
  id, i.e. from the content. TagLib hands you whatever string the tag contains,
  which is frequently wrong and, in the ID3v2 `-->` case, denotes a URL rather
  than image data. Hence sniffing magic bytes first and only falling back to the
  tag's claim when it at least starts with `image/`. If neither holds, no art is
  reported, which is a behaviour change from always emitting something.
- **First picture wins.** Matches the previous behaviour, which took the first
  stream with `AV_DISPOSITION_ATTACHED_PIC`.

## Release checklist

When publishing the binary, the release notes need a line naming TagLib, the
version bundled, and MPL-1.1 as the licence it is used under, with a link to the
matching upstream tarball. That link is what discharges the obligation; under
LGPL-2.1 you would instead owe relinkable object files, which is precisely the
reason for choosing MPL. The README already carries the same note.

## Housekeeping

Delete this file before merging.
