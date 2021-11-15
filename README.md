# mpv-mpris

`mpv-mpris` is a plugin for mpv which allows control of the player using
standard media keys. 

This plugin implements the MPRIS D-Bus interface and can be controlled using
tools such as [playerctl](https://github.com/acrisci/playerctl) or through many
open source desktop environments, such as GNOME and KDE.

## Note

This plugin requires mpv to be built with `--enable-cplugins` (default as of mpv 0.26)
and to be built with Lua support (to enable loading scripts).

## Load

mpv will load the plugin from the scripts dirs:

- `/etc/mpv/scripts`: for all users
- `~/.config/mpv/scripts`: for current user

mpv can load it from other locations:

```
mpv --script=/path/to/mpris.so video.mp4
```

## Install

Packages are available in a several [distributions](https://repology.org/project/mpv-mpris/versions).

For 64-bit x86 Linux a pre-built version is [available](https://github.com/hoyon/mpv-mpris/releases)
and can be copied into one of the mpv scripts dirs documented above.

The self-built `mpris.so` file can be installed with `make install` and will
be installed to the appropriate mpv scripts dir for your current user or
to the mpv system wide scripts dir for all users when you install as root.

## Build

Build requirements:
 - C99 compiler (gcc or clang)
 - pkg-config
 - mpv development files
 - glib development files
 - gio development files

Building should be as simple as running `make` in the source code directory.

## D-Bus interfaces

Implemented:
- `org.mpris.MediaPlayer2` 
- `org.mpris.MediaPlayer2.Player` 

Not implemented:
- `org.mpris.MediaPlayer2.TrackList`
 - `org.mpris.MediaPlayer2.Playlists`
