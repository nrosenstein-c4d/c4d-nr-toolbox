VERSION=$(shell git describe --tags)

ifeq ($(OS),Windows_NT)
	PLATFORM=win
else
	PLATFORM=mac
endif

.PHONY: dist
dist:
	mkdir -p dist
	tar -zcvf dist/c4d-nr-toolbox-$(VERSION)-rXX-$(PLATFORM).tar.gz \
		--exclude=*.lib --exclude=*.exp --exclude=*.ilk --exclude=*.pdb \
		--exclude=build --exclude=*.pyc \
		--transform='s/plugin/c4d-nr-toolbox-$(VERSION)/' \
		include plugin python LICENSE.txt README.md
