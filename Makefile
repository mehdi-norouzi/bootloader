all: crc.o list_bin.o main.o
	gcc -g -o bootloader main.o crc.o list_bin.o

.c.o:
	gcc -c -g $<

run: bootloader
	./bootloader

clean: 
	rm bootloader *.o
