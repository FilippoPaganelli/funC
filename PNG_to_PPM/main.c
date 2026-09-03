#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>

const uint8_t png_signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
const uint32_t IHDR = 0x52444849;
const uint32_t IEND = 0x444E4549;
const uint32_t IDAT = 0x54414449;

typedef struct
{
    uint8_t *data;
    size_t size;
} Buffer;

void read_bytes_or_exit(FILE *file, void *buffer, size_t buffer_size)
{
    size_t bytes_read = fread(buffer, 1, buffer_size, file);
    if (bytes_read != buffer_size)
    {
        printf("ERROR: An error occurred while reading the file\n");
        exit(1);
    }
}

void write_bytes_or_exit(FILE *file, void *buffer, size_t buffer_size)
{
    size_t bytes_written = fwrite(buffer, 1, buffer_size, file);
    if (bytes_written != buffer_size)
    {
        printf("ERROR: An error occurred while writing to the file\n");
        exit(1);
    }
}

void print_bytes(uint8_t *buffer, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; i++)
    {
        printf("%u ", buffer[i]);
    }
    printf("\n");
}

void reverse_bytes(void *_buffer, size_t buffer_size)
{
    uint8_t *buffer = _buffer;
    for (size_t i = 0; i < buffer_size / 2; i++)
    {
        uint8_t a = buffer[i];
        buffer[i] = buffer[buffer_size - i - 1];
        buffer[buffer_size - i - 1] = a;
    }
}

void skip_data(FILE *file, uint32_t size)
{
    if (fseek(file, size, SEEK_CUR) < 0)
    {
        printf("ERROR: Could not skip chunk data.\n");
        exit(1);
    }
}

void skip_CRC(FILE *file)
{
    if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
    {
        printf("ERROR: Could not skip chunk CRC\n");
        exit(1);
    }
}

void handle_IHDR_chunk(FILE *file, uint8_t *data, uint32_t *width, uint32_t *height)
{
    read_bytes_or_exit(file, data, 13);
    reverse_bytes(data, 13);

    memcpy(width, data + 9, sizeof(*width));
    memcpy(height, data + 5, sizeof(*height));
    printf("  INFO: width:%u height:%u\n", *width, *height);

    uint8_t bit_depth, color_type, compr_met, filter_met, interl_met = 0;
    memcpy(&bit_depth, data + 4, sizeof(uint8_t));
    memcpy(&color_type, data + 3, sizeof(uint8_t));
    memcpy(&compr_met, data + 2, sizeof(uint8_t));
    memcpy(&filter_met, data + 1, sizeof(uint8_t));
    memcpy(&interl_met, data, sizeof(uint8_t));
    printf("  INFO: bit depth:%u, color type:%u, compr method:%u, filter method:%u, interl method:%u\n", bit_depth, color_type, compr_met, filter_met, interl_met);
}

void handle_IDAT_chunk(FILE *file, uint8_t *data, uint32_t data_size)
{
    read_bytes_or_exit(file, data, data_size);
    printf("  INFO: Read %u bytes from an IDAT chunk\n", data_size);
}

int decompress(const uint8_t *data,
               size_t data_size,
               uint8_t **out,
               size_t *out_size)
{
    z_stream strm = {0};

    if (inflateInit(&strm) != Z_OK)
        return -1;

    size_t capacity = 4096;
    size_t size = 0;

    uint8_t *buffer = malloc(capacity);
    if (!buffer)
    {
        inflateEnd(&strm);
        return -1;
    }

    strm.next_in = (Bytef *)data;
    strm.avail_in = data_size;

    for (;;)
    {
        /* If output buffer is full, make it bigger. */
        if (size == capacity)
        {
            capacity *= 2;

            uint8_t *tmp = realloc(buffer, capacity);
            if (!tmp)
            {
                free(buffer);
                inflateEnd(&strm);
                return -1;
            }

            buffer = tmp;
        }

        strm.next_out = buffer + size;
        strm.avail_out = capacity - size;

        int ret = inflate(&strm, Z_NO_FLUSH);

        /* How many bytes did inflate write? */
        size = capacity - strm.avail_out;

        if (ret == Z_STREAM_END)
            break;

        if (ret != Z_OK)
        {
            free(buffer);
            inflateEnd(&strm);
            return -1;
        }
    }

    inflateEnd(&strm);

    *out = buffer;
    *out_size = size;

    return 0;
}

void write_PPM_image(uint8_t *data, size_t data_size, uint32_t width, uint32_t height)
{
    const char *file_path = "output.ppm";
    FILE *file = fopen(file_path, "wb");
    if (file == NULL)
    {
        printf("ERROR: Could not open file \"%s\"\n", file_path);
        exit(1);
    }

    write_bytes_or_exit(file, "P6\n", 3);
    char w_and_h[50];
    sprintf(w_and_h, "%u %u\n", width, height);
    write_bytes_or_exit(file, w_and_h, strlen(w_and_h));
    write_bytes_or_exit(file, "255\n", 4);
    write_bytes_or_exit(file, data, data_size);

    printf("INFO: Written PPM output file at \"%s\"\n", file_path);

    fclose(file);
}

/* ---------- MAIN ---------- */
int main(int argc, char **argv)
{
    (void)argc;
    char *program = *argv++;
    if (*argv == NULL)
    {
        printf("ERROR: A PNG file path must be provided for \"%s\"\n", program);
        exit(1);
    }

    char *file_path = *argv;
    printf("INFO: Analyzing file \"%s\"\n", file_path);
    FILE *input_file = fopen(file_path, "rb");
    if (input_file == NULL)
    {
        printf("ERROR: Could not open file \"%s\"\n", file_path);
        exit(1);
    }

    // PNG signature bytes
    uint8_t signature_bytes[8];
    read_bytes_or_exit(input_file, signature_bytes, sizeof(signature_bytes));
    if (memcmp(signature_bytes, png_signature, sizeof(png_signature)) != 0)
    {
        printf("ERROR: PNG signature bytes are incorrect\n");
        exit(1);
    }
    printf("INFO: Read correct PNG signature: ");
    print_bytes(signature_bytes, sizeof(signature_bytes));

    // Chunks
    bool end = false;
    Buffer buffer = {0};
    uint32_t width, height = 0;
    printf("------------------------------\n");
    while (!end)
    {
        // Chunk size
        uint32_t size;
        read_bytes_or_exit(input_file, &size, sizeof(size));
        reverse_bytes(&size, sizeof(size));
        printf("Chunk size: %u\n", size);

        // Chunk type
        uint8_t type[4];
        read_bytes_or_exit(input_file, type, sizeof(type));
        printf("Chunk type: %.*s (0x%08X)\n", 4, type, *(uint32_t *)type);

        // Chunk data
        if (*(uint32_t *)type == IHDR)
        {
            uint8_t IHDR_data[13]; // always 13 bytes
            handle_IHDR_chunk(input_file, IHDR_data, &width, &height);
        }
        else if (*(uint32_t *)type == IEND)
        {
            skip_data(input_file, size);
            end = true;
        }
        else if (*(uint32_t *)type == IDAT)
        {
            if (buffer.size > 0) // found more than 1 IDAT chunks
            {
                size_t new_size = buffer.size + size;
                buffer.data = realloc(buffer.data, new_size);
            }
            buffer.data = malloc(size);
            buffer.size = size;
            handle_IDAT_chunk(input_file, buffer.data, size);
        }
        else
        {
            skip_data(input_file, size);
        }

        // Chunk CRC
        skip_CRC(input_file);

        printf("------------------------------\n");
    }

    uint8_t *inflated;
    size_t inflated_size = 0;
    decompress(buffer.data, buffer.size, &inflated, &inflated_size);

    // Specific image's decompression strategy
    // [1 byte filter type][1041 × 4 bytes RGBA] <- each line (= 4165 bytes)
    // the number of rows is the image height (= 729 here)
    // here I have filter=0, so no operations to perform on the RGBA bytes
    printf("INFO: Image data inflated from size %zu to %zu\n", buffer.size, inflated_size);
    printf("INFO: Image width: %u, height: %u\n", width, height);

    size_t total_data_size = width * height * 3; // Dropping A, keeping RGB
    uint8_t final_data[total_data_size];
    uint8_t *inflated_begin_ptr = inflated;
    size_t final_data_index = 0;
    size_t index = 0;
    while (index < inflated_size)
    {
        if ((index) % (width * 4 + 1) == 0)
        {
            // Skip filter byte
            index++;
        }
        uint8_t r = inflated[index++];
        uint8_t g = inflated[index++];
        uint8_t b = inflated[index++];
        // Skip A (= Alpha) byte
        index++;
        final_data[final_data_index++] = r;
        final_data[final_data_index++] = g;
        final_data[final_data_index++] = b;
    }

    free(buffer.data);
    free(inflated_begin_ptr);
    fclose(input_file);

    // Write PPM output file
    write_PPM_image(final_data, sizeof(final_data), width, height);

    return 0;
}