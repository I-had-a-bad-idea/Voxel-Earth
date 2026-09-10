CXX = g++
CC  = gcc

ENET_DIR = external/enet
CURL_DIR = external/curl
VoxelVGL = external/VoxelVGL

CFLAGS = -isystem$(ENET_DIR)/include -Wall -Wextra -Wno-unused-parameter -g -O3
CXXFLAGS = $(CFLAGS) -std=c++20

VoxelVGL_INCLUDE = \
	-isystem$(VoxelVGL)/include \
	-isystem$(VoxelVGL)/include/ktx/include

CPPFLAGS = \
	-I. \
	-isystemexternal \
	-isystem$(CURL_DIR)/include \
	$(VoxelVGL_INCLUDE)

LIBS = \
	-L$(CURL_DIR)/build/lib \
	-lcurl \
	-lws2_32 \
	-lwinmm \
	-L$(VULKAN_SDK)/Lib \
	-Lexternal/VoxelVGL \
	-lvulkan-1 \
	-l:VGL.a \
	-lSDL3 \
	-lslang \


# Sources
ENET_SRC = \
	$(ENET_DIR)/callbacks.c \
	$(ENET_DIR)/compress.c \
	$(ENET_DIR)/host.c \
	$(ENET_DIR)/list.c \
	$(ENET_DIR)/packet.c \
	$(ENET_DIR)/peer.c \
	$(ENET_DIR)/protocol.c \
	$(ENET_DIR)/win32.c

CLIENT_SRC = \
	$(wildcard client/*.cpp) \
	$(wildcard client/World-Scene/*.cpp)

SHARED_SRC = \
	$(wildcard Math/*.cpp) \
	$(wildcard World/*.cpp) \
	$(wildcard network/*.cpp)

SERVER_SRC = \
	$(wildcard server/*.cpp)

# Object files
ENET_OBJ   = $(ENET_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SHARED_OBJ = $(SHARED_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)

CLIENT_DEPS = $(CLIENT_OBJ:.o=.d) $(SHARED_OBJ:.o=.d) $(ENET_OBJ:.o=.d)
SERVER_DEPS = $(SERVER_OBJ:.o=.d) $(SHARED_OBJ:.o=.d) $(ENET_OBJ:.o=.d)


# Targets

all: server.exe client.exe


server.exe: $(SERVER_OBJ) $(SHARED_OBJ) $(ENET_OBJ)
	$(CXX) $^ $(LIBS) -o $@


client.exe: $(CLIENT_OBJ) $(SHARED_OBJ) $(ENET_OBJ)
	$(CXX) $^ $(LIBS) -o $@


# Compilation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@


# Automatically generated header dependencies
-include $(CLIENT_DEPS)
-include $(SERVER_DEPS)


# Run
server: server.exe
	./server.exe

client: client.exe
	./client.exe

# Clean
clean:
	del /Q *.exe 2>nul
	for /R %%f in (*.o *.d) do del /Q "%%f" 2>nul


.PHONY: all server client clean