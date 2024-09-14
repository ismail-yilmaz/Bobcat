.PHONY: build

build:
	mkdir -p build
	sed 's/c++14/c++17/g' 3p/umk/CLANG.bm  >> 3p/umk/CLANG_CPP17.bm
	3p/umk/umk ./,3p/uppsrc Bobcat 3p/umk/CLANG_CPP17.bm -brvh +GUI,SHARED build/Bobcat
	mv build/Bobcat build/bobcat

build-web:
	mkdir -p build
	sed 's/c++14/c++17/g' 3p/umk/CLANG.bm  >> 3p/umk/CLANG_CPP17.bm
	3p/umk/umk ./,3p/uppsrc Bobcat 3p/umk/CLANG_CPP17.bm -brvh +WEBGUI,SHARED build/Bobcat
	mv build/Bobcat build/bobcat

download:
	mkdir -p 3p/download
	wget https://github.com/ultimatepp/ultimatepp/releases/download/2023.2/uppsrc-17045.tar.gz -P 3p/download
	wget https://github.com/ultimatepp/ultimatepp/releases/download/2023.2/umk-17045-linux-x86-64.tar.gz -P 3p/download
	tar -xf 3p/download/uppsrc-17045.tar.gz -C 3p
	tar -xf 3p/download/umk-17045-linux-x86-64.tar.gz -C 3p

run:
	build/bobcat

clean:
	rm -rf build
	rm -rf 3p

