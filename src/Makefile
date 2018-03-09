all: default

default:
	cd ./vendor/hiredis/ && $(MAKE)
	mkdir -p bin
	gcc -fPIC -shared -I./include ./miva-redis.cpp ./vendor/hiredis/libhiredis.a -o ./bin/miva-redis.so
	cp ./bin/miva-redis.so /builtins