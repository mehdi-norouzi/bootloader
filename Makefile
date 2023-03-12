all: main.c
	gcc main.c -o bootloader

run: bootloader
	./bootloader

clean: 
	rm bootloader
