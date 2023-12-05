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

metadata_test = find_program('test/metadata')
pause_test = find_program('test/pause')
play_test = find_program('test/play')
play_pause_test = find_program('test/play-pause')
stop_test = find_program('test/stop')
quit_test = find_program('test/quit')
wrapper = find_program('dbus-run-session')
setup = find_program('test/setup')
env = find_program('test/env')

test('metadata-test', wrapper, args: ['--', metadata_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)
test('pause-test', wrapper, args: ['--', pause_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)
test('play-test', wrapper, args: ['--', play_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)
test('play-pause-test', wrapper, args: ['--', play_pause_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)
test('stop-test', wrapper, args: ['--', stop_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)
test('quit-test', wrapper, args: ['--', quit_test.full_path(), setup.full_path(), env.full_path(), mpris_lib.full_path()], timeout: 60)

