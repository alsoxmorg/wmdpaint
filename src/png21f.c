#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>
#include <arpa/inet.h>


//#define OLDMAGIC "1bfarbf\0"
#define    MAGIC "1fpirate"
#define OLDMAGIC "1bfarbfe"

#include <unistd.h>
// Get luminance from RGBA
static inline uint8_t is_white(png_byte r, png_byte g, png_byte b) {
    int lum = (int)(0.299*r + 0.587*g + 0.114*b);
    return lum > 127; // threshold at 128
}

int main(int argc, char **argv) {
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

  /*
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.png output.1f\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen input");
        return 1;
    }
  */
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return 1;

    png_infop info = png_create_info_struct(png);
    if (!info) return 1;

    if (setjmp(png_jmpbuf(png))) return 1;

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth  = png_get_bit_depth(png, info);

    // Convert all to RGBA
    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    png_bytep *row_pointers = malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    fclose(fp);

    // Output
    /*FILE *out = fopen(argv[2], "wb");*/
    FILE *out = stdout;
    if (!out) {
        perror("fopen output");
        return 1;
    }

    fwrite(MAGIC, 1, 8, out);

    uint32_t net_w = htonl(width);
    uint32_t net_h = htonl(height);
    fwrite(&net_w, 4, 1, out);
    fwrite(&net_h, 4, 1, out);

    int row_bytes = (width + 7) / 8;
    uint8_t *packed_row = malloc(row_bytes);

    for (int y = 0; y < height; y++) {
        memset(packed_row, 0, row_bytes);
        png_bytep row = row_pointers[y];

        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]); // RGBA
            int bit = is_white(px[0], px[1], px[2]);
            packed_row[x / 8] |= bit << (7 - (x % 8));
        }

        fwrite(packed_row, 1, row_bytes, out);
    }

    fclose(out);

    // Cleanup
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    free(packed_row);
    png_destroy_read_struct(&png, &info, NULL);

    return 0;
}
