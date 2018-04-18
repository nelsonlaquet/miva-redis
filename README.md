# miva-redis
Redis client for Miva. This only supports running on Linux.

# Development
Simply run "docker-compose up" in this directory to spool up a dev environment for working on this module. Access the wwwroot folder by going to http://localhost:8080. All .mv files in the wwwroot directory will be compiled by the mivascript compiler whenever a change is detected. All .mv files have access to the miva-redis module.

## Containers
- cpp-compiler
	- Watches for changes to the src/**/*.cpp files, and runs "make" when changes are directed. Output is set to ./src/bin, and also copied to the /data/builtins directory
- miva-compiler
	- Watches for changes to the wwwroot/**/*.mv files, and compiles them into .mvc files on change. References the /data/builtins directory so that it can see the compiled miva-redis binary.
- miva-empresa
	- An apache server configured with the empresa engine in CGI mode, seving out the ./wwwroot directory. References the /data/builtins directory so that it can execute the miva-redis binary.
- redis
	- An instance of a redis server available in the miva-empresa container using the hostname "redis".

# Usage
1) Compile for your target architecture/linux kernel/distro
2) Either:
	- Copy `miva-redis.so` into your builtins directory for your mivavm OR
	- Add `<BUILTIN-LIB LIBRARY = "/path/to/miva-redis.so">` to your `mivavm.conf` file
3) Add a `redis.dat` file to your `mivadata` directory with either of the formats:
	- `host:port` OR
	- `host:port:db` where `db` is the database index to connect to
4) Congrats! You can now use the `redis_*` commands! miva-redis will use the `redis.dat` file to automatically connect to the server the first time you try to use a `redis_*` command. If `redis.dat` doesn't exist, or there is an error, all `redis_*` commands will fail silently.

# Functions

## Low Level

### `redis_reply redis_command(string command, string args)`
**command**: the command format to send to redis. You can use ? as substitions.

**args**: a comma seperated list of variable names, all starting with either `l.` or `g.`, that will be substituted into ? placeholders for the command.

Returns `0` on error, otherwise returns a redis_reply. See `formatRedisReply` and https://github.com/redis/hiredis#using-replies for more information.

#### Examples
```html
<MvAssign name="l.data" value="HEY THERE!" />
<MvAssign name="l._" value="{redis_command('SET key ?', 'l.data')}" />
```

```html
<MvAssign name="l.data" value="{redis_command('GET key', '')}" />
<MvEval expr="{l.data:string}" />
```

### `void redis_command_append(string command, string args)`
See `redis_command` and https://github.com/redis/hiredis#pipelining

### `int redis_error(string* message)`
**message**: if there is an error, this variable is filled with the error message.

If there is an error, returns a non-zero error code. Otherwise, returns `0`.

### `void redis_error_clear()`
Clears the last redis error.

### `int redis_free()`
Disconnects from redis. Returns `1` on success, `0` on failure. Note that you *shouldn't* have to call this function. I don't know why I made it.

### `int redis_get_reply(reply var)`
See https://github.com/redis/hiredis#pipelining

### `int redis_is_enabled()`
If a redis conneciton has not yet been attempted, attempts to connect using the information provided in `mivadata/redis.dat`. If no `mivadata/redis.dat` file is present, or the connection fails, returns `0`.

If a redis connection has been attempted, returns `1` or `0` depending on if the connection was successful.

## High Level

### `int redis_append(string key, string* value)`
Wrapper around [APPEND](https://redis.io/commands/append).

**key**: the key to append to.
**value**: a reference to the string to append.

Returns `0` on error, `1` on success.

### `int redis_del(string key)`
Wrapper around [DEL](https://redis.io/commands/del).

**key**: the key to delete (todo: accept arrays too).

Returns `0` on error, `1` on success.

### `int redis_get(string key, string* ret)`
Wrapper around [GET](https://redis.io/commands/get).

**key**: the key to get.
**ret**: the value of the key will be placed in this variable.

Returns `0` on error, `-1` if the key was not found, and `1` if the key was found.

### `int redis_set(string key, string* value)`
Wrapper around [SET](https://redis.io/commands/set).

**key**: the key to set.
**value**: a reference to the string to set the key to.

Returns `0` on error, `1` on success.

### `int redis_setex(string key, string value var, int expires)`
Wrapper around [SETEX](https://redis.io/commands/setex).

**key**: the key to set.
**value**: a reference to the string to set the key to.
**expires**: the expiration of the key in seconds.