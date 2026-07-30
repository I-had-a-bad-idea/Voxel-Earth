CC = gcc

ENET_DIR = external/enet

CFLAGS = -I$(ENET_DIR)/include -Wall -Wextra -Wno-unused-parameter -g

ENET_SRC = \
	$(ENET_DIR)/callbacks.c \
	$(ENET_DIR)/compress.c \
	$(ENET_DIR)/host.c \
	$(ENET_DIR)/list.c \
	$(ENET_DIR)/packet.c \
	$(ENET_DIR)/peer.c \
	$(ENET_DIR)/protocol.c \
	$(ENET_DIR)/win32.c

LIBS = -lws2_32 -lwinmm

all: server.exe client.exe

server.exe: server.c
	$(CC) $(CFLAGS) server.c $(ENET_SRC) $(LIBS) -o server.exe

client.exe: client.c
	$(CC) $(CFLAGS) client.c $(ENET_SRC) $(LIBS) -o client.exe

server: server.exe
	server.exe

client: client.exe
	client.exe

clean:
	del /Q *.exe 2>nul

.PHONY: all server client clean