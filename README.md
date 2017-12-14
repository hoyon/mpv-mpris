# mpv-mpris
MPRIS plugin for mpv written in C. Requires mpv to be built with `--enable-cplugins` (default as of mpv 0.26).

Implements `org.mpris.MediaPlayer2` and `org.mpris.MediaPlayer2.Player` D-Bus interfaces.

## Build

If you are using 64bit Linux a prebuilt version is available [here](https://github.com/hoyon/mpv-mpris/releases) otherwise you will have to manually build

Build requirements:
 - C99 compiler (gcc or clang)
 - pkg-config
 - mpv development files
 - glib development files
 - gio development files

Building should be as simple as running `make` in the cloned directory.

## Install

To install either run `make install` or copy the compiled `mpris.so` file into the directory `~/.config/mpv/scripts`. 

The plugin can be used without installing by running mpv with the `--script` flag:

```
mpv --script /path/to/mpris.so video.mp4
```

## TODO
 - `org.mpris.MediaPlayer2.TrackList`
 - `org.mpris.MediaPlayer2.Playlists`
