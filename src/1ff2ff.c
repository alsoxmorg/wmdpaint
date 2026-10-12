#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define    MAGIC "1fpirate"
#define OLDMAGIC "1bfarbfe"
//1bfarbf
static void
writeuint16(uint16_t v, FILE *fp) {
  uint8_t buf[2];
  buf[0] = v >> 8;
  buf[1] = v & 0xff;
  fwrite(buf, 1, 2, fp);
}

static void
writeuint32(uint32_t v, FILE *fp) {
  uint8_t buf[4];
  buf[0] = (v >> 24) & 0xff;
  buf[1] = (v >> 16) & 0xff;
  buf[2] = (v >> 8) & 0xff;
  buf[3] = v & 0xff;
  fwrite(buf, 1, 4, fp);
}

int main(int argc, char *argv[]) {
  /*if (argc != 2) {
    fprintf(stderr, "Usage: %s <input.1f>\n", argv[0]);
    return 1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (!fp) {
    perror("fopen");
    return 1;
    }*/
    FILE *fp = NULL;

    if (argc > 1) {
    fp = fopen(argv[1], "rb");
    if (!fp) {
    perror("fopen");
    return 1;
    }
    } else if (!isatty(fileno(stdin))) {
    // stdin is being piped in                                                                                                                                                    
    fp = stdin;
    } else {
    fprintf(stderr, "Usage: %s <file.1f>\nOr pipe data via stdin\n", argv[0]);
    return 1;
    }


  
  char magic[8];
  fread(magic, 1, 8, fp);
  if (memcmp(magic, MAGIC, 8) != 0) {
    if (memcmp(magic, OLDMAGIC, 8) != 0) {
      fprintf(stderr, "%s: Invalid 1f file magic (%s)\n", argv[0], magic);
      fclose(fp);
      return 1;
    }
  }

  uint32_t width, height;
  fread(&width, 4, 1, fp);
  fread(&height, 4, 1, fp);
  width = ntohl(width);
  height = ntohl(height);

  int row_bytes = (width + 7) / 8;
  uint8_t *bitmap = malloc(row_bytes * height);
  if (!bitmap) {
    perror("malloc");
    fclose(fp);
    return 1;
  }
  fread(bitmap, 1, row_bytes * height, fp);
  fclose(fp);

  // Write farbfeld header to stdout
  fwrite("farbfeld", 1, 8, stdout);
  writeuint32(width, stdout);
  writeuint32(height, stdout);

  // Each pixel is RGBA16: write 0x0000 or 0xffff depending on bitmap bit
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      int byte_index = y * row_bytes + (x / 8);
      int bit_index = 7 - (x % 8);
      int bit = (bitmap[byte_index] >> bit_index) & 1;
      uint16_t val = bit ? 0xffff : 0x0000;
      for (int i = 0; i < 3; i++) // R, G, B
	writeuint16(val, stdout);
      writeuint16(0xffff, stdout); // Alpha
    }
  }

  free(bitmap);
  return 0;
}
