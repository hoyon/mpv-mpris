project('mpv-mpris', 'c', version: '1.1', default_options: [
  'c_std=c99',
  'warning_level=3',
])

gio = dependency('gio-2.0')
gio_unix = dependency('gio-unix-2.0')
glib = dependency('glib-2.0')
mpv = dependency('mpv')
ffmpeg = dependency('libavformat')

fs = import('fs')

if get_option('install_user')
  lib_install_dir = fs.expanduser(get_option('scripts_dir'))
else
  lib_install_dir = join_paths(get_option('prefix'), get_option('libdir'), get_option('plugindir'))
endif

mpris_lib = library(
  'mpris',
  'mpris.c',
  pic: true,
  install_dir: lib_install_dir,
  install: true,
  dependencies: [
    gio,
    gio_unix,
    glib,
    mpv,
    ffmpeg
  ]  
)

if not get_option('install_user')
  install_symlink('mpris.so', pointing_to: join_paths(lib_install_dir, fs.name(mpris_lib.full_path())), install_dir: get_option('sys_scripts_dir'))
endif

subdir('test')
