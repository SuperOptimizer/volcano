#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
//#include <zlib.h>

#include "minilibs.h"
#include "miniutils.h"
#include "miniz.h"

#define MAX_LINE_LENGTH 1024
#define MAX_HEADER_LINES 100

typedef struct {
    char type[32];
    int dimension;
    char space[32];
    int sizes[16];
    float space_directions[16][3];
    char endian[16];
    char encoding[32];
    float space_origin[3];

    size_t data_size;
    void* data;

    bool is_valid;
} nrrd_t;





PRIVATE bool parse_sizes(char* value, nrrd_t* nrrd) {
    char* token = strtok(value, " ");
    int i = 0;
    while (token != NULL && i < nrrd->dimension) {
        nrrd->sizes[i] = atoi(token);
        if (nrrd->sizes[i] <= 0) {
            printf("Invalid size value: %s", token);
            return false;
        }
        token = strtok(NULL, " ");
        i++;
    }
    return i == nrrd->dimension;
}

PRIVATE bool parse_space_directions(char* value, nrrd_t* nrrd) {
    char* token = strtok(value, ") (");
    int i = 0;
    while (token != NULL && i < nrrd->dimension) {
        if (strcmp(token, "none") == 0) {
            nrrd->space_directions[i][0] = 0;
            nrrd->space_directions[i][1] = 0;
            nrrd->space_directions[i][2] = 0;
        } else {
            if (sscanf(token, "%f,%f,%f",
                      &nrrd->space_directions[i][0],
                      &nrrd->space_directions[i][1],
                      &nrrd->space_directions[i][2]) != 3) {
                printf("Invalid space direction: %s", token);
                return false;
            }
        }
        token = strtok(NULL, ") (");
        i++;
    }
    return true;
}

PRIVATE bool parse_space_origin(char* value, nrrd_t* nrrd) {
    // Remove parentheses
    value++; // Skip first '('
    value[strlen(value)-1] = '\0'; // Remove last ')'

    if (sscanf(value, "%f,%f,%f",
               &nrrd->space_origin[0],
               &nrrd->space_origin[1],
               &nrrd->space_origin[2]) != 3) {
        printf("Invalid space origin: %s", value);
        return false;
    }
    return true;
}

PRIVATE size_t get_type_size(const char* type) {
    if (strcmp(type, "uint8") == 0 || strcmp(type, "uchar") == 0) return 1;
    if (strcmp(type, "uint16") == 0) return 2;
    if (strcmp(type, "uint32") == 0) return 4;
    if (strcmp(type, "float") == 0) return 4;
    if (strcmp(type, "double") == 0) return 8;
    return 0;
}

PRIVATE bool read_raw_data(FILE* fp, nrrd_t* nrrd) {
    size_t bytes_read = fread(nrrd->data, 1, nrrd->data_size, fp);
    if (bytes_read != nrrd->data_size) {
        printf("Failed to read data: expected %zu bytes, got %zu",
                nrrd->data_size, bytes_read);
        return false;
    }
    return true;
}

PRIVATE bool read_gzip_data(FILE* fp, nrrd_t* nrrd) {
    z_stream strm = {0};
    unsigned char in[16384];
    size_t bytes_written = 0;

    if (inflateInit2(&strm,-MAX_WBITS) != Z_OK) {
        printf("Failed to initialize zlib");
        return false;
    }

    int ret;
    do {
        strm.avail_in = fread(in, 1, sizeof(in), fp);
        if (ferror(fp)) {
            inflateEnd(&strm);
            printf("Error reading compressed data");
            return false;
        }
        if (strm.avail_in == 0) break;
        strm.next_in = in;

        do {
            strm.avail_out = nrrd->data_size - bytes_written;
            strm.next_out = (unsigned char*)nrrd->data + bytes_written;
            ret = inflate(&strm, Z_NO_FLUSH);

            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&strm);
                printf("Decompression error: %s", strm.msg);
                return false;
            }

            bytes_written = nrrd->data_size - strm.avail_out;

        } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return true;
}

PUBLIC nrrd_t* nrrd_read(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("could not open %s\n",filename);
        return NULL;
    }

    nrrd_t* nrrd = calloc(1, sizeof(nrrd_t));
    if (!nrrd) {

        printf("could not allocate ram for nrrd\n");
        fclose(fp);
        return NULL;
    }
    nrrd->is_valid = true;

    char line[MAX_LINE_LENGTH];
    if (!fgets(line, sizeof(line), fp)) {
        printf("Failed to read magic");
        nrrd->is_valid = false;
        goto cleanup;
    }
    trim(line);

    if (!str_starts_with(line, "NRRD")) {
        printf("Not a NRRD file: %s", line);
        nrrd->is_valid = false;
        goto cleanup;
    }

    while (fgets(line, sizeof(line), fp)) {
        trim(line);

        // Empty line marks end of header
        if (strlen(line) == 0) break;

        //if we are left with just a newline after trimming then we have a blank line, we are going to
        // start reading data now so we need to break
        if(line[0] == '\n') break;

        // Skip comments
        if (line[0] == '#') continue;

        char* separator = strchr(line, ':');
        if (!separator) continue;

        *separator = '\0';
        char* key = line;
        char* value = separator + 1;
        while (*value == ' ') value++;

        trim(key);
        trim(value);

        if (strcmp(key, "type") == 0) {
            strncpy(nrrd->type, value, sizeof(nrrd->type)-1);
        }
        else if (strcmp(key, "dimension") == 0) {
            nrrd->dimension = atoi(value);
            if (nrrd->dimension <= 0 || nrrd->dimension > 16) {
                printf("Invalid dimension: %d", nrrd->dimension);
                nrrd->is_valid = false;
                goto cleanup;
            }
        }
        else if (strcmp(key, "space") == 0) {
            strncpy(nrrd->space, value, sizeof(nrrd->space)-1);
        }
        else if (strcmp(key, "sizes") == 0) {
            if (!parse_sizes(value, nrrd)) {
                nrrd->is_valid = false;
                goto cleanup;
            }
        }
        else if (strcmp(key, "space directions") == 0) {
            if (!parse_space_directions(value, nrrd)) {
                nrrd->is_valid = false;
                goto cleanup;
            }
        }
        else if (strcmp(key, "endian") == 0) {
            strncpy(nrrd->endian, value, sizeof(nrrd->endian)-1);
        }
        else if (strcmp(key, "encoding") == 0) {
            strncpy(nrrd->encoding, value, sizeof(nrrd->encoding)-1);
        }
        else if (strcmp(key, "space origin") == 0) {
            if (!parse_space_origin(value, nrrd)) {
                nrrd->is_valid = false;
                goto cleanup;
            }
        }
    }

    size_t type_size = get_type_size(nrrd->type);
    if (type_size == 0) {
        printf("Unsupported type: %s", nrrd->type);
        nrrd->is_valid = false;
        goto cleanup;
    }

    nrrd->data_size = type_size;
    for (int i = 0; i < nrrd->dimension; i++) {
        nrrd->data_size *= nrrd->sizes[i];
    }

    nrrd->data = malloc(nrrd->data_size);
    if (!nrrd->data) {
        printf("Failed to allocate %zu bytes", nrrd->data_size);
        nrrd->is_valid = false;
        goto cleanup;
    }

    if (strcmp(nrrd->encoding, "raw") == 0) {
        if (!read_raw_data(fp, nrrd)) {
            nrrd->is_valid = false;
            goto cleanup;
        }
    }
    else if (strcmp(nrrd->encoding, "gzip") == 0) {
        if (!read_gzip_data(fp, nrrd)) {
            nrrd->is_valid = false;
            goto cleanup;
        }
    }
    else {
        printf("Unsupported encoding: %s", nrrd->encoding);
        nrrd->is_valid = false;
        goto cleanup;
    }

cleanup:
    fclose(fp);
    if (!nrrd->is_valid) {
        if (nrrd->data) free(nrrd->data);
        free(nrrd);
        return NULL;
    }
    return nrrd;
}

PUBLIC nrrd_free(nrrd_t* nrrd) {
    if (nrrd) {
        if (nrrd->data) free(nrrd->data);
        free(nrrd);
    }
}
