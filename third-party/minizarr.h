#pragma once


#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "json.h/json.h"


typedef struct zarr_compressor_settings {
  int32_t blocksize;
  int32_t clevel;
  char cname[32];
  char id[32];
  int32_t shuffle;
} zarr_compressor_settings;

typedef struct zarr_metadata {
  int32_t shape[3];
  int32_t chunks[3];
  zarr_compressor_settings compressor;
  char dtype[8];
  int32_t fill_value;
  char order; // Single character 'C' or 'F'
  int32_t zarr_format;
} zarr_metadata;

PRIVATE struct json_value_s *find_value(const struct json_object_s *obj, const char *key) {
  struct json_object_element_s *element = obj->start;
  while (element) {
    if (element->name->string_size == strlen(key) &&
        strncmp(element->name->string, key, element->name->string_size) == 0) {
      return element->value;
    }
    element = element->next;
  }
  return NULL;
}

PRIVATE void parse_int32_array(struct json_array_s *array, int32_t output[3]) {
  struct json_array_element_s *element = array->start;
  for (int i = 0; i < 3 && element; i++) {
    struct json_number_s *num = element->value->payload;
    output[i] = (int32_t) strtol(num->number, NULL, 10);
    element = element->next;
  }
}

PRIVATE int parse_zarr_metadata(const char *json_string, zarr_metadata *metadata) {
  struct json_value_s *root = json_parse(json_string, strlen(json_string));
  if (!root) {
    printf("Failed to parse JSON!\n");
    return 0;
  }

  struct json_object_s *object = root->payload;

  struct json_value_s *shapes_value = find_value(object, "shape");
  if (shapes_value && shapes_value->type == json_type_array) {
    parse_int32_array(shapes_value->payload, metadata->shape);
  }

  struct json_value_s *chunks_value = find_value(object, "chunks");
  if (chunks_value && chunks_value->type == json_type_array) {
    parse_int32_array(chunks_value->payload, metadata->chunks);
  }

  struct json_value_s *compressor_value = find_value(object, "compressor");
  if (compressor_value && compressor_value->type == json_type_object) {
    struct json_object_s *compressor = compressor_value->payload;

    struct json_value_s *blocksize = find_value(compressor, "blocksize");
    if (blocksize && blocksize->type == json_type_number) {
      struct json_number_s *num = blocksize->payload;
      metadata->compressor.blocksize = (int32_t) strtol(num->number, NULL, 10);
    }

    struct json_value_s *clevel = find_value(compressor, "clevel");
    if (clevel && clevel->type == json_type_number) {
      struct json_number_s *num = clevel->payload;
      metadata->compressor.clevel = (int32_t) strtol(num->number, NULL, 10);
    }

    struct json_value_s *cname = find_value(compressor, "cname");
    if (cname && cname->type == json_type_string) {
      struct json_string_s *str = cname->payload;
      strncpy(metadata->compressor.cname, str->string, sizeof(metadata->compressor.cname) - 1);
    }

    struct json_value_s *id = find_value(compressor, "id");
    if (id && id->type == json_type_string) {
      struct json_string_s *str = id->payload;
      strncpy(metadata->compressor.id, str->string, sizeof(metadata->compressor.id) - 1);
    }

    struct json_value_s *shuffle = find_value(compressor, "shuffle");
    if (shuffle && shuffle->type == json_type_number) {
      struct json_number_s *num = shuffle->payload;
      metadata->compressor.shuffle = (int32_t) strtol(num->number, NULL, 10);
    }
  }

  struct json_value_s *dtype_value = find_value(object, "dtype");
  if (dtype_value && dtype_value->type == json_type_string) {
    struct json_string_s *str = dtype_value->payload;
    strncpy(metadata->dtype, str->string, sizeof(metadata->dtype) - 1);
  }

  struct json_value_s *fill_value = find_value(object, "fill_value");
  if (fill_value && fill_value->type == json_type_number) {
    struct json_number_s *num = fill_value->payload;
    metadata->fill_value = (int32_t) strtol(num->number, NULL, 10);
  }

  struct json_value_s *order_value = find_value(object, "order");
  if (order_value && order_value->type == json_type_string) {
    struct json_string_s *str = order_value->payload;
    metadata->order = str->string[0];
  }

  struct json_value_s *format_value = find_value(object, "zarr_format");
  if (format_value && format_value->type == json_type_number) {
    struct json_number_s *num = format_value->payload;
    metadata->zarr_format = (int32_t) strtol(num->number, NULL, 10);
  }

  free(root);
  return 1;
}


PUBLIC zarr_metadata parse_zarray(char *path) {
  zarr_metadata metadata = {0};

  FILE *fp = fopen(path, "rt");
  if (fp == NULL) {
    printf("could not open file %s\n", path);
    assert(false);
    return metadata;
  }
  s32 size;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = calloc(size + 1, 1);
  fread(buf, 1, size, fp);


  if (parse_zarr_metadata(buf, &metadata)) {
    printf("Shape: [%d, %d, %d]\n",
           metadata.shape[0], metadata.shape[1], metadata.shape[2]);
    printf("Chunks: [%d, %d, %d]\n",
           metadata.chunks[0], metadata.chunks[1], metadata.chunks[2]);
    printf("Compressor:\n");
    printf("  blocksize: %d\n", metadata.compressor.blocksize);
    printf("  clevel: %d\n", metadata.compressor.clevel);
    printf("  cname: %s\n", metadata.compressor.cname);
    printf("  id: %s\n", metadata.compressor.id);
    printf("  shuffle: %d\n", metadata.compressor.shuffle);
    printf("dtype: %s\n", metadata.dtype);
    printf("fill_value: %d\n", metadata.fill_value);
    printf("order: %c\n", metadata.order);
    printf("zarr_format: %d\n", metadata.zarr_format);
  }

  free(buf);
  return metadata;
}

