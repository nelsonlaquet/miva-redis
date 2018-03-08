all: default

default:
	mkdir -p bin
	cd vendor/hiredis
	make
	cd ../../
	gcc -fPIC -shared -I./src/include ./src/miva-redis.c ./vendor/hiredis/libhiredis.a -o ./bin/miva-redis.so