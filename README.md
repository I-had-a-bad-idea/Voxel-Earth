# Voxel-Earth

Multiplayer voxel game, that uses real-world height data.   
Basically a voxel game, but the world is the Earth.

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

### Compiling the actual project

Run:

```bash
make
```

You will have a `client.exe` and a `server.exe`

## Licenses
[Terrain Tiles License](./TERRAIN_TILES_ATTRIBUTION.md)