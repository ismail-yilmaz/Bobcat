.PHONY: build

build:
	mkdir -p build
	3p/umk/umk ./,3p/uppsrc Bobcat 3p/umk/CLANG.bm -brvh +GUI,SHARED build/Bobcat
	mv build/Bobcat build/bobcat

build-web:
	mkdir -p build
	3p/umk/umk ./,3p/uppsrc Bobcat 3p/umk/CLANG.bm -brvh +WEBGUI,SHARED build/Bobcat
	mv build/Bobcat build/bobcat

build-headless:
	mkdir -p build
	3p/umk/umk ./,3p/uppsrc Bobcat 3p/umk/CLANG.bm -brvh +SDLGUI,SHARED build/Bobcat
	mv build/Bobcat build/bobcat

download:
	mkdir -p 3p/download
	wget https://github.com/ultimatepp/ultimatepp/releases/download/v2025.1.1/uppsrc-17810.tar.gz -P 3p/download
	wget https://github.com/ultimatepp/ultimatepp/releases/download/v2024.1/umk-17458-linux-x86-64.tar.gz -P 3p/download
	tar -xf 3p/download/uppsrc-17810.tar.gz -C 3p
	tar -xf 3p/download/umk-17458-linux-x86-64.tar.gz -C 3p

run:
	build/bobcat

clean:
	rm -rf build
	rm -rf 3p

