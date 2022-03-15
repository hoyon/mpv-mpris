MAKEFLAGS += --output-sync=target

tests = \
	metadata \
	pause \
	play \
	play-pause \
	stop \
	quit

.PHONY: \
	test \
	$(tests) \
	clean

test: $(tests)

$(tests):
	./wrapper "$@"

clean:
	rm -f \
	  *.mpv.ipc* \
	  *.mpv.log \
	  *.xvfb.log \
	  *.Xauthority \
	  *.socat.log \
	  *.exit-code.log \
	  *.stderr.log
	rm -rf dbus
