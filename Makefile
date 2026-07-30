CXX = C:\msys64\ucrt64\bin\g++.exe
CC  = C:\msys64\ucrt64\bin\gcc.exe

ENET_DIR = external/enet

CFLAGS = -I$(ENET_DIR)/include -Wall -Wextra -Wno-unused-parameter -g

LIBS = -lws2_32 -lwinmm

ENET_SRC = \
	$(ENET_DIR)/callbacks.c \
	$(ENET_DIR)/compress.c \
	$(ENET_DIR)/host.c \
	$(ENET_DIR)/list.c \
	$(ENET_DIR)/packet.c \
	$(ENET_DIR)/peer.c \
	$(ENET_DIR)/protocol.c \
	$(ENET_DIR)/win32.c

# Renderer submodule
RENDERER_DIR = external/Rasterization-Renderer

RENDERER_INCLUDE = \
	-I$(RENDERER_DIR)/Helper \
	-I$(RENDERER_DIR)/Math \
	-I$(RENDERER_DIR)/Object \
	-I$(RENDERER_DIR)/Rendering \
	-I$(RENDERER_DIR)/Scenes \
	-I$(RENDERER_DIR)/Textures


SDL_DIR = C:/msys64/ucrt64
SDL_INCLUDE = -I$(SDL_DIR)/include
SDL_LIB = -L$(SDL_DIR)/lib

RENDERER_SRC = \
	$(wildcard $(RENDERER_DIR)/*.cpp) \
	$(wildcard $(RENDERER_DIR)/Helper/*.cpp) \
	$(wildcard $(RENDERER_DIR)/Math/*.cpp) \
	$(wildcard $(RENDERER_DIR)/Object/*.cpp) \
	$(wildcard $(RENDERER_DIR)/Rendering/*.cpp) \
	$(wildcard $(RENDERER_DIR)/Scenes/*.cpp)


CLIENT_DIR = client

CLIENT_SRC = \
	$(wildcard $(CLIENT_DIR)/*.cpp) \
	$(wildcard $(CLIENT_DIR)/World-Scene/*.cpp)


all: server.exe client.exe

server.exe: server/server.c
	$(CC) $(CFLAGS) server/server.c $(ENET_SRC) $(LIBS) -o server.exe

client.exe: ${CLIENT_SRC} $(RENDERER_SRC)
	${CXX} $(CFLAGS) \
		-I. \
		${SDL_LIB} \
		$(SDL_INCLUDE) \
		$(RENDERER_INCLUDE) \
		${CLIENT_SRC} \
		$(ENET_SRC) \
		$(RENDERER_SRC) \
		-lws2_32 \
		-lwinmm \
		-lSDL2 \
		-lSDL2_image \
		-lopengl32 \
		-lgdi32 \
		-mconsole \
		-o client.exe

server: server.exe
	server.exe

client: client.exe
	client.exe

clean:
	del /Q *.exe 2>nul

.PHONY: all server client clean