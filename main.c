#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define CHUNK_SIZE 1024
#define MAX_RETRIES 5

#define IGNORE 0

// Calculate 16-bit CRC for chunk data
static uint16_t calc_crc(unsigned char *data, int size) {
#define CRC16 0x8005
  uint16_t out = 0;
  int bits_read = 0, bit_flag;
  /* Sanity check: */
  if (data == NULL)
    return 0;
  while (size > 0) {
    bit_flag = out >> 15;
    /* Get next bit: */
    out <<= 1;
    out |= (*data >> (7 - bits_read)) & 1;
    /* Increment bit counter: */
    bits_read++;
    if (bits_read > 7) {
      bits_read = 0;
      data++;
      size--;
    }
    /* Cycle check: */
    if (bit_flag) {
      out ^= CRC16;
    }
  }
  return out;
}

int main(int argc, char *argv[]) {
  int fd;
  struct termios serial_settings;
  unsigned char chunk_data[CHUNK_SIZE + 2]; // Include space for 16-bit CRC
  int bytes_read, bytes_written, retries;
  unsigned short crc, expected_crc;
  unsigned char ack;

  // Open binary file
  FILE *fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    perror("Error opening binary file");
    exit(1);
  }

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

#if IGNORE
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
#endif // IGNORE
  }

  // Close binary file and serial port
  fclose(fp);
  close(fd);
  return 0;
}