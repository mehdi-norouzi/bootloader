#include "crc.h"
#include "list_bin.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <termios.h>
#include <unistd.h>

#define CHUNK_SIZE 255
#define MAX_RETRIES 5

#define IGNORE 0

typedef struct __attribute__((__packed__)) {
  uint8_t address;
  uint8_t category;
  uint8_t context;
  uint16_t crc;
} packet_StructTypedef;

typedef struct __attribute__((__packed__)) {
  uint8_t address;
  uint8_t caetgory;
  uint8_t context;
  uint8_t data[CHUNK_SIZE];
  uint16_t crc;
} data_StructTypedef;

typedef enum {
  START_FLASH = 0x06,
  END_FLASH = 0x07,
  WRITE_FLASH = 0x08,
  EXIT_BOOTLOADER = 3,
  START_BOOTLOADER = 4,
} commands_EnumTypedef;

static packet_StructTypedef pack_command(uint8_t address, uint8_t context) {
  packet_StructTypedef commandPacket = {
      .address = address,
      .category = 0x03,
      .context = context,
  };
  commandPacket.crc =
      calc_crc((uint8_t *)&commandPacket, (sizeof commandPacket) - 2);
  return commandPacket;
}

static void bootloader() {
  printf("=====================================\r\n"
         "        Bootloader Started\r\n"
         "=====================================\r\n\r\n");
  int fd;
  struct termios serial_settings;
  unsigned char chunk_data[CHUNK_SIZE + 2]; // Include space for 16-bit CRC
  int bytes_read, bytes_written, retries;
  unsigned short crc, expected_crc;
  unsigned char ack;

  // Open binary file
  /* FILE *fp = fopen("./binaries/fw.bin", "rb");
  if (fp == NULL) {
    perror("Error opening binary file");
    exit(1);
  } */

  // Open serial port
  fd = open("/dev/ttyUSB0", O_RDWR);
  if (fd < 0) {
    perror("Error opening serial port");
    exit(1);
  }

  // Set serial port parameters
  memset(&serial_settings, 0, sizeof(serial_settings));
  serial_settings.c_cflag = CS8 | CREAD | CLOCAL;
  serial_settings.c_iflag = 0;
  serial_settings.c_oflag = 0;
  serial_settings.c_lflag = 0;
  cfsetispeed(&serial_settings, B57600);
  cfsetospeed(&serial_settings, B57600);
  tcsetattr(fd, TCSANOW, &serial_settings);

  printf("Enter the address of the device: \r\n");
  uint8_t addrString[1];
  uint8_t contextString[1];
  scanf("%s", addrString);
  uint8_t addr = atoi((char *)addrString);

  while (1) {
    printf("6: Wipe flash area\r\n7: Write to flash\r\n8: End flashing "
           "process\r\nEnter the command: ");
    scanf("%s", contextString);
    uint8_t context = atoi((char *)contextString);
    packet_StructTypedef command = pack_command(addr, context);
    sleep(1);
    write(fd, (uint8_t *)&command, sizeof command);
    // memset(&fd, 0, sizeof fd);
    close(fd);
    fd = open("/dev/ttyUSB0", O_WRONLY);
    close(fd);
    sleep(10);
    fd = open("/dev/ttyUSB0", O_RDONLY);
    packet_StructTypedef response;
    while (1) {
      int res = read(fd, &response, 5);
      if (res == 5) {
        printf("%u, %u, %u, %u\r\n", response.address, response.category,
               response.context, response.crc);
      } else {
        printf("read err: %d\r\n", res);
        break;
      }
    }
    break;
    /* char **files = find_bin_files();
    if (files == NULL) {
      printf("No binary files found\r\n");
    } else {
      uint8_t i = 0;
      while (files[i] != NULL) {
        printf("%d: %s\r\n", i, files[i]);
        i++;
      }
    } */
  }
}

int main(int argc, char *argv[]) {

  printf("s -- start flashing process\r\nq -- quit\r\n\r\nEnter a command: ");
  char command[100];
  scanf("%s", command);
  if (strlen(command) > 1) {
    perror("Error: Invalid command\r\n");
    exit(1);
  }
  if (command[0] == 's') {
    bootloader();
  }
  while (command[0] != 'q') {
    printf("quiting...\r\n");
    exit(0);
  }

#if IGNORE
  uint8_t count = 0;
  // Send file in 1K chunks with CRC
  while ((bytes_read = fread(chunk_data, 1, CHUNK_SIZE, fp)) > 0) {
    // Calculate CRC for chunk data
    crc = calc_crc(chunk_data, bytes_read);

    // Append CRC to chunk data
    chunk_data[bytes_read] = crc & 0xFF;
    chunk_data[bytes_read + 1] = (crc >> 8) & 0xFF;

    printf("crc%u: %u\r\n", count, crc);
    count++;

    // Send chunk data
    retries = 0;
    do {
      bytes_written = write(fd, chunk_data, bytes_read + 2);
      if (bytes_written != bytes_read + 2) {
        perror("Error sending chunk data");
        exit(1);
      }

      // Wait for ACK/NACK
      expected_crc = crc;
      ack = 0;
      while (ack == 0) {
        if (read(fd, &ack, 1) == 1) {
          if (ack == 'A') {
            printf("ACK received\n");
            break;
          } else if (ack == 'N') {
            printf("NACK received\n");
            if (retries < MAX_RETRIES) {
              printf("Retrying...\n");
              retries++;
              break;
            } else {
              printf("Max retries exceeded. Aborting.\n");
              exit(1);
            }
          } else {
            printf("Invalid packet\n");
            ack = 0;
          }
        }
      }
      if (ack == 'A' && expected_crc != crc) {
        printf("CRC error detected. Retrying...\n");
        ack = 0;
        retries++;
      }
    } while (ack != 'A' && retries < MAX_RETRIES);
    if (retries == MAX_RETRIES) {
      printf("Max retries exceeded. Aborting.\n");
      exit(1);
    }
  }
#endif // IGNORE

  // Close binary file and serial port
  // fclose(fp);
  // close(fd);
  return 0;
}
