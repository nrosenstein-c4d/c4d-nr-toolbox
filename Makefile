VERSION=$(shell git describe --tags)

ifeq ($(OS),Windows_NT)
	PLATFORM=win
else
	PLATFORM=mac
endif

.PHONY: dist
dist:
	mkdir -p dist
	tar -zcvf dist/c4d-nr-toolbox-$(VERSION)-r20-$(PLATFORM).tar.gz \
		--exclude=*.lib --exclude=*.exp --exclude=*.ilk --exclude=*.pdb \
		--exclude=build --exclude=*.pyc \
		docs examples include python res scripts \
		default_features.cfg LICENSE.txt README.md \
		$(shell ls c4d-nr-toolkit.xdl64 c4d-nr-toolkit.xlib)
