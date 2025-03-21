FILES = ./initialize_swift_net.c ./swift_net.c ./send_packet.c ./handle_packets.c ./append_to_packet.c ./generic_functions.c ./initialize_client_socket.c ./initialize_server_socket.c
CC = clang
CFLAGS = -O0
OBJ_DIR = ../build
OBJS = $(foreach file, $(FILES), $(OBJ_DIR)/$(file:.c=.o))

all:
	$(foreach file, $(FILES), $(CC) $(CFLAGS) -c $(file) -o $(OBJ_DIR)/$(file:.c=.o);)
	ar rcs ../build/libswift_net.a $(OBJS);

clean:
	$(foreach file, $(wildcard $(OBJ_DIR)/*.o), rm $(file);)
