# Simple TCP Client

This is a very basic TCP client written in C that connects to a server.

## How to Build

You can build the client using the provided build script:

```
./build.sh
build.bat(windows)
```

Or manually with gcc:

```
gcc -o client client.c
gcc -o client.exe client.c (Windows)
```

## How to Run

After building, run the client:

```
./client
```

## What it Does

1. Connects to a server at 127.0.0.1:8080
2. Lets you send a message to the server
3. Displays the server's response
4. Disconnects

## Note

Make sure a server is running at 127.0.0.1:8080 before connecting.
```

