all:
	clang -fsanitize=address -g -O0 -v main.c -o main -L../../../build/client -lswift_net_client 
	sudo ./main
