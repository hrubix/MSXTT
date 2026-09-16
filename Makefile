# MSXTT — public build entry

.PHONY: all build clean help com

all: build

help:
	@echo Targets: build clean com help
	@echo Output: emul\dos2\msxtt.com emul\dsk\msxtt.dsk

build:
	@build.bat

com: build
	@if not exist build mkdir build
	@if exist emul\dos2\msxtt.com copy /Y emul\dos2\msxtt.com build\msxtt.com

clean:
	@if exist out rmdir /S /Q out
	@if exist emul\dsk rmdir /S /Q emul\dsk
	@if exist build\msxtt.com del /Q build\msxtt.com
