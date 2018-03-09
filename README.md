# miva-redis
Redis client for Miva.

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