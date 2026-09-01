#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

const uint8_t png_signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
const uint32_t IHDR = 0x52444849;
const uint32_t IEND = 0x444E4549;
const uint32_t IDAT = 0x54414449;

typedef struct
{
    uint8_t *data;
    size_t size;
    size_t capacity;
} Buffer;

void read_bytes(FILE *file, void *buffer, size_t buffer_size)
{
    size_t bytes_read = fread(buffer, 1, buffer_size, file);
    if (bytes_read != buffer_size)
    {
        printf("ERROR: An error occurred while reading the file\n");
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

bool found_data_chunk_and_read(FILE *file, uint8_t *type, uint8_t *data, uint32_t data_size)
{
    printf("  INFO: READ\n");

    bool found_data_chunk = false;
    read_bytes(file, &data, data_size); // TODO: maybe reverse bytes?

    // printf("  INFO: READ %.*s\n", 4, type);
    if (*(uint32_t *)type == IHDR)
    {
        reverse_bytes(&data, data_size);

        uint32_t width, height = 0;
        memcpy(&width, data + 9, sizeof(width));
        memcpy(&height, data + 5, sizeof(height));
        printf("  INFO: width:%u height:%u\n", width, height);

        uint8_t bit_depth, color_type, compr_met, filter_met, interl_met = 0;
        memcpy(&bit_depth, data + 4, sizeof(uint8_t));
        memcpy(&color_type, data + 3, sizeof(uint8_t));
        memcpy(&compr_met, data + 2, sizeof(uint8_t));
        memcpy(&filter_met, data + 1, sizeof(uint8_t));
        memcpy(&interl_met, data, sizeof(uint8_t));
        printf("  INFO: bit depth:%u, color type:%u, compr method:%u, filter method:%u, interl method:%u\n", bit_depth, color_type, compr_met, filter_met, interl_met);
    }
    else if (*(uint32_t *)type == IDAT)
    {
        found_data_chunk = true;
    }
    else
    {
        // Skip chunk data
        if (fseek(file, data_size, SEEK_CUR) < 0)
        {
            printf("ERROR: Could not skip chunk data for %.*s\n", 4, type);
            exit(1);
        }
        printf("  INFO: Skipping chunk\n");
    }

    // Always skip chunk CRC
    if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
    {
        printf("ERROR: Could not skip chunk CRC for %.*s\n", 4, type);
        exit(1);
    }
    printf("DONE READING\n");

    return found_data_chunk;
}

int main(int argc, char **argv)
{
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
    read_bytes(input_file, signature_bytes, sizeof(signature_bytes));
    if (memcmp(signature_bytes, png_signature, sizeof(png_signature)) != 0)
    {
        printf("ERROR: PNG signature bytes are incorrect\n");
        exit(1);
    }
    printf("INFO: Read correct PNG signature: ");
    print_bytes(signature_bytes, sizeof(signature_bytes));

    // Chunks
    bool reached_end = false;
    Buffer buffer = {0};
    while (!reached_end)
    {
        uint32_t chunk_size;
        read_bytes(input_file, &chunk_size, sizeof(chunk_size));
        reverse_bytes(&chunk_size, sizeof(chunk_size));
        printf("Chunk size: %u\n", chunk_size);

        uint8_t chunk_type[4];
        read_bytes(input_file, chunk_type, sizeof(chunk_type));
        printf("Chunk type: %.*s (0x%08X)\n", 4, chunk_type, *(uint32_t *)chunk_type);

        uint8_t chunk_data[chunk_size];
        bool found_data_chunk = found_data_chunk_and_read(input_file, chunk_type, chunk_data, chunk_size);
        printf("%u\n", found_data_chunk);
        if (found_data_chunk)
        {
        }

        printf("------------------------------\n");
    }

    fclose(input_file);

    return 0;
}