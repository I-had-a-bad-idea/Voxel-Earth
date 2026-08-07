CXX = g++
CC  = gcc

ENET_DIR = external/enet

CFLAGS = -I$(ENET_DIR)/include -Wall -Wextra -Wno-unused-parameter -g

LIBS = \
	-lws2_32 \
	-lwinmm \
	-L$(VULKAN_SDK)/Lib \
	-Lexternal/VulkanGraphicsLib \
	-lvulkan-1 \
	-l:VulkanGraphicsLib.a \
	-lSDL3 \
	-lslang


ENET_SRC = \
	$(ENET_DIR)/callbacks.c \
	$(ENET_DIR)/compress.c \
	$(ENET_DIR)/host.c \
	$(ENET_DIR)/list.c \
	$(ENET_DIR)/packet.c \
	$(ENET_DIR)/peer.c \
	$(ENET_DIR)/protocol.c \
	$(ENET_DIR)/win32.c

VULKAN_GRAPHICS_LIB = external/VulkanGraphicsLib

VULKAN_GRAPHICS_LIB_INCLUDE = \
	-I$(VULKAN_GRAPHICS_LIB)/include \
	-I$(VULKAN_GRAPHICS_LIB)/external/ \
	-I$(VULKAN_GRAPHICS_LIB)/external/ktx/include \
	-I$(VULKAN_GRAPHICS_LIB)/external/ktx/other_include


VULKAN_SDK = C:/VulkanSDK/1.4.357.0

CLIENT_DIR = client

CLIENT_SRC = \
	$(wildcard $(CLIENT_DIR)/*.cpp) \
	$(wildcard $(CLIENT_DIR)/World-Scene/*.cpp)


all: server.exe client.exe

server.exe: server/server.c
	$(CC) $(CFLAGS) server/server.c $(ENET_SRC) $(LIBS) -o server.exe

client.exe: ${CLIENT_SRC}
	${CXX} $(CFLAGS) \
		${CLIENT_SRC} \
		-I. \
		$(VULKAN_GRAPHICS_LIB_INCLUDE) \
		-I$(VULKAN_SDK)/Include \
		$(ENET_SRC) \
		${LIBS} \
		-o client.exe

server: server.exe
	server.exe

client: client.exe
	client.exe

clean:
	del /Q *.exe 2>nul

.PHONY: all server client clean