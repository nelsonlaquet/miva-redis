all: default

default:
	cd ./vendor/hiredis/ && $(MAKE)
	mkdir -p bin
	gcc -fPIC -shared -I./src/include ./src/miva-redis.cpp ./vendor/hiredis/libhiredis.a -o ./bin/miva-redis.so