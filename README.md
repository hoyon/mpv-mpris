# mpv-mpris

`mpv-mpris` is a plugin for mpv which allows control of the player using
standard media keys. 

This plugin implements the MPRIS D-Bus interface and can be controlled using
tools such as [playerctl](https://github.com/acrisci/playerctl) or through many
open source desktop environments, such as GNOME and KDE.

## Compatibility

This plugin requires mpv to be built with `--enable-cplugins` (default as of mpv 0.26)
and to be built with Lua support (to enable loading scripts).

## Loading

mpv will automatically load the plugin from the following directories:

- `/etc/mpv/scripts`: for all users
- `~/.config/mpv/scripts`: for current user

mpv can also manually load the plugin from other directories:

```
mpv --script=/path/to/mpris.so video.mp4
```

## Install

Packages are available for many [distributions](https://repology.org/project/mpv-mpris/versions).

For 64-bit x86 Linux a pre-built version is [available here](https://github.com/hoyon/mpv-mpris/releases)
and can be copied into one of the mpv scripts directories documented above.

A self-built `mpris.so` file can be installed with `make install` and will
be installed to the appropriate mpv scripts directory for your current user
or to the mpv system wide scripts directory for all users when you install as root.

## Build

Build requirements:
 - C99 compiler (gcc or clang)
 - pkg-config
 - mpv development files
 - glib development files
 - gio development files
 - libavformat development files

Building should be as simple as running `make` in the source code directory.

## Test

Test requirements:
 - mpv (for loading the mpv mpris plugin)
 - mpv-mpris plugin (installed or self-built)
 - playerctl (for sending MPRIS commands via D-Bus)
 - dbus-send (from dbus, for sending MPRIS commands via D-Bus)
 - sound-theme-freedesktop (for a file to play in mpv)
 - bash (for running the test scripts)
 - dbus-run-session (from dbus, for simulating a D-Bus session)
 - xvfb and xauth (for simulating an X11 session)
 - jq (for mpv IPC JSON generation and parsing)
 - socat (for passing JSON to/from mpv IPC sockets)
 - awk (for redirecting parts of mpv stderr logs)

Testing should be as simple as running `make test` in the source code directory.

The stderr of the tests will be empty unless there are mpv/etc issues.

The tests accept these environment variables as parameters:
 - `MPV_MPRIS_TEST_PLUGIN`: the mpv mpris plugin file to test, must be
   readable and executable, defaults to the self-built one. Set it to an
   empty string to only load and test an already installed mpv mpris plugin.
 - `MPV_MPRIS_TEST_PLAY`: the file to play during tests, defaults to
   `/usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga`.
   Use a file at most 10 seconds long or the tests will take a long time.
 - `MPV_MPRIS_TEST_MPV_IPC`: dir for mpv IPC, default is test dir.
 - `MPV_MPRIS_TEST_DBUS`: dir for D-Bus sockets, default is test dir.
   Sets the `XDG_RUNTIME_DIR` env var so D-Bus uses the dir.
 - `MPV_MPRIS_TEST_XAUTH`: dir for Xauthority files, default is test dir.
 - `MPV_MPRIS_TEST_LOG`: dir for mpv/socat logs, default is test dir.
 - `MPV_MPRIS_TEST_TMP`: dir for temp files, default is test dir.
   Sets the `TEMPDIR`, `TMPDIR`, `TEMP` and `TMP` env vars.
 - `MPV_MPRIS_TEST_NO_STDERR`: disable extra printing of the errors printed
   to stderr. This is for when the test scenario already does this.

These parameters are useful for running the tests in alternate test scenarios.

## D-Bus interfaces

Implemented:
- `org.mpris.MediaPlayer2` 
- `org.mpris.MediaPlayer2.Player` 

Not implemented:
- `org.mpris.MediaPlayer2.TrackList`
- `org.mpris.MediaPlayer2.Playlists`
