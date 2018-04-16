all: default

default:
	cd ./vendor/hiredis/ && $(MAKE)
	node ./miva-redis-commands.g.cpp.js
	mkdir -p bin
	gcc -fPIC -D_GLIBCXX_USE_CXX11_ABI=0 -shared -I./include ./miva-redis.cpp ./miva-redis-commands.g.cpp ./vendor/hiredis/libhiredis.a -o ./bin/miva-redis.so
	cp ./bin/miva-redis.so /builtins