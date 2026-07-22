MAKEFLAGS += --output-sync=target

tests = \
	metadata \
	pause \
	play \
	play-pause \
	stop \
	quit

# Only meaningful for a plugin built with TagLib, so kept out of the default run.
taglib_tests = \
	embedded-art

.PHONY: \
	test \
	test-taglib \
	$(tests) \
	$(taglib_tests) \
	clean

test: $(tests)

test-taglib: $(taglib_tests)

$(tests) $(taglib_tests):
	./wrapper "$@"

clean:
	rm -f \
	  embedded-art.mp3 \
	  *.mpv.ipc* \
	  *.mpv.log \
	  *.xvfb.log \
	  *.Xauthority \
	  *.socat.log \
	  *.exit-code.log \
	  *.stderr.log
	rm -rf dbus
