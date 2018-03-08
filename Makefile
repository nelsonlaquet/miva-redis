all: default

default:
	$(CC) -fPIC -shared -I./src/include miva-redis.c -o miva-redis.so