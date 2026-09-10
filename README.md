# Voxel-Earth

Multiplayer voxel game, that uses real-world height data.   
Basically a voxel game, but the world is the Earth and you can play with your friends on it.

## Compilation
### Building curl (once)

Configuring curl build:

```bash
cmake -S external/curl -B external/curl/build ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=ON ^
    -DBUILD_CURL_EXE=OFF ^
    -DBUILD_TESTING=OFF ^
    -DCURL_USE_SCHANNEL=ON ^
    -DCURL_USE_OPENSSL=OFF ^
    -DCURL_ZLIB=OFF ^
    -DCURL_BROTLI=OFF ^
    -DCURL_ZSTD=OFF ^
    -DCURL_USE_LIBPSL=OFF ^
    -DUSE_NGHTTP2=OFF ^
    -DUSE_NGHTTP3=OFF ^
    -DUSE_IDN2=OFF
```

Building curl:

```bash
cmake --build external/curl/build --config Release
```

Find where CMake put the built curl and edit the Makefile accrodingly (replace the `-L$(CURL_DIR)/build/lib` with whatever you need)
Seems like you also have to put the `libcurl.dll` file next to the executable.

### Compiling the actual project

Run:

```bash
make
```

You will have a `client.exe` and a `server.exe`.


## Running

1. [Compile the project](#compilation)
2. Execute the `server.exe` and keep it open
3. Execute the `client.exe`
4. Tell your friend to execute the `client.exe`
5. (See that he cant connect because currently servers are only locally)
6. End client/server with `Esc` (never just close the server or you might lose the changes made to the world)

## Licenses
[Terrain Tiles License](./TERRAIN_TILES_ATTRIBUTION.md)