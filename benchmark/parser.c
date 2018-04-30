#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSER_BUFFER_SIZE 1024

void null_terminate_line(char *line, int *len);

void parse_line(char *line, int *line_length);
int parse_binary(char *token, unsigned char *buf, int size);
char parse_hex(char *token);
char char_to_binary(char x);

char *read_data_file(const char *file_name, InputMalloc allocator) {
  FILE *input_file = fopen(file_name, "rb");

  if (input_file == NULL) {
    fprintf(stderr, "Data file not found\n");
    exit(1);
  }

  fseek(input_file, 0, SEEK_END);
  int input_file_size = ftell(input_file);
  rewind(input_file);

  char *file_contents = allocator(input_file_size);
  fread(file_contents, sizeof(char), input_file_size, input_file);

  fclose(input_file);

  return file_contents;
}

void parse_pattern_file(const char *file_name, DFC_PATTERN_INIT *pattern_init,
                        AddPattern add_pattern) {
  FILE *file = fopen(file_name, "rt");
  if (!file) {
    fprintf(stderr, "Could not open pattern file");
    exit(1);
  }

  char line[PARSER_BUFFER_SIZE];

  int pattern_id = 0;
  while (fgets(line, PARSER_BUFFER_SIZE, file) != NULL) {
    int pattern_length = 0;
    null_terminate_line(line, &pattern_length);

    parse_line(line, &pattern_length);

    add_pattern(pattern_init, (unsigned char *)line, pattern_length, 0,
                pattern_id);
    ++pattern_id;
  }
}

void null_terminate_line(char *line, int *len) {
  *len = 0;
  while ((line[*len] != '\0') && (line[*len] != '\n')) {
    ++(*len);
  }
  line[*len] = '\0';
}

void parse_line(char *output_pattern, int *pattern_length) {
  unsigned char buf[PARSER_BUFFER_SIZE];
  char *duplicated_pattern = strdup(output_pattern);
  char *pattern = duplicated_pattern;
  int size = 0;

  int should_parse_binary = false;
  char *token;
  while ((token = strsep(&pattern, "|"))) {
    if (!should_parse_binary) {
      memcpy(buf + size, token, strlen(token));
      size += strlen(token);
    } else {
      size = parse_binary(token, buf, size);
    }

    if (pattern) {
      should_parse_binary = !should_parse_binary;
    }
  }

  buf[size] = '\0';
  memcpy(output_pattern, buf, size);
  *pattern_length = size;

  free(duplicated_pattern);
}

int parse_binary(char *token, unsigned char *buf, int size) {
  char temp[PARSER_BUFFER_SIZE];
  size_t i = 0;
  int c = 0;
  while (i < strlen(token)) {
    temp[c] = parse_hex(token + i);
    c++;
    i += 3;
  }

  temp[c] = '\0';
  memcpy(buf + size, temp, c);
  size = size + c;

  return size;
}

char parse_hex(char *token) {
  char x = token[0];
  char y = token[1];

  x = char_to_binary(x);
  y = char_to_binary(y);

  return x * 16 + y;
}

char char_to_binary(char x) {
  if ((x >= '0') && (x <= '9')) {
    return x - '0';
  } else if ((x >= 'A') && (x <= 'F')) {
    return x - 'A' + 10;
  } else {
    fprintf(stderr, "Pattern parsing error!\n");
    exit(1);
  }
}
