# mpv-mpris

`mpv-mpris` is a plugin for mpv which allows control of the player using
standard media keys. 

This plugin implements the MPRIS D-Bus interface and can be controlled using
tools such as [playerctl](https://github.com/acrisci/playerctl) or through many
Linux DEs, such as Gnome and KDE.

## Build

If you are using 64bit Linux a pre-built version is available
[here](https://github.com/hoyon/mpv-mpris/releases) otherwise you will have to
manually build.

Build requirements:
 - C99 compiler (gcc or clang)
 - pkg-config
 - mpv development files
 - glib development files
 - gio development files

Building should be as simple as running `make` in the cloned directory.

## Install

To install either run `make install` or copy the compiled `mpris.so` file into
one of the following directories:
- `/etc/mpv/scripts`
- `~/.config/mpv/scripts`

The plugin can be used without installing by running mpv with the `--script` flag:

```
mpv --script=/path/to/mpris.so video.mp4
```

## Notes

This plugin requires mpv to be built with `--enable-cplugins` (default as of mpv 0.26)
and to be built with Lua support (to enable loading scripts).

### D-Bus interfaces

Implemented:
- `org.mpris.MediaPlayer2` 
- `org.mpris.MediaPlayer2.Player` 

Not implemented:
- `org.mpris.MediaPlayer2.TrackList`
 - `org.mpris.MediaPlayer2.Playlists`
