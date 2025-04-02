#include "http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim_whitespace(char *str) {
  while (isspace((unsigned char)*str)) str++;
  if (*str == '\0') return str;

  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;
  *(end + 1) = '\0';

  return str;
}

ssize_t httprequest_parse_headers(HTTPRequest *req, char *buffer,
                                  ssize_t buffer_len) {
  if (!req || !buffer || buffer_len <= 0) return -1;
  buffer[buffer_len - 1] = '\0';
  char *saveptr;
  char *action = strtok_r(buffer, " ", &saveptr);
  char *path = strtok_r(NULL, " ", &saveptr);
  char *version = strtok_r(NULL, "\r\n", &saveptr);

  if (!action || !path || !version) {
    fprintf(stderr, "Error parsing request line.\n");
    return -1;
  }

  req->action = strdup(action);
  req->path = strdup(path);
  req->version = strdup(version);

  if (!req->action || !req->path || !req->version) {
    fprintf(stderr, "Memory allocation failed for request line components.\n");
    return -1;
  }

  req->header_count = 0;
  req->headers = (Header *)malloc(sizeof(Header) * 10);
  if (!req->headers) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  char *line;
  int allocated_headers = 10;
  while ((line = strtok_r(NULL, "\r\n", &saveptr)) && *line) {
    char *key_saveptr;
    char *key = strtok_r(line, ":", &key_saveptr);
    char *value = strtok_r(NULL, "", &key_saveptr);

    if (key && value) {
      value = trim_whitespace(value);

      if (req->header_count >= allocated_headers) {
        allocated_headers *= 2;
        req->headers =
            realloc(req->headers, sizeof(Header) * allocated_headers);
        if (!req->headers) {
          perror("realloc");
          exit(EXIT_FAILURE);
        }
      }

      req->headers[req->header_count].key = strdup(trim_whitespace(key));
      req->headers[req->header_count].value = strdup(value);

      if (!req->headers[req->header_count].key ||
          !req->headers[req->header_count].value) {
        fprintf(stderr, "Memory allocation failed for header key or value.\n");
        return -1;
      }

      req->header_count++;
    }
  }

  char *payload = strtok_r(NULL, "", &saveptr);
  if (payload) {
    req->payload = strdup(payload);
    if (!req->payload) {
      fprintf(stderr, "Memory allocation failed for payload.\n");
      return -1;
    }
  } else {
    req->payload = NULL;
  }

  return 0;
}

ssize_t httprequest_read(HTTPRequest *req, int sockfd) {
  if (!req) return -1;

  ssize_t total_bytes = 0;
  ssize_t buffer_size = 1024;
  char *buffer = (char *)malloc(buffer_size);
  if (!buffer) {
    perror("malloc");
    return -1;
  }

  ssize_t bytes_read;
  int headers_done = 0;
  char *headers_end = NULL;

  while (!headers_done) {
    if (total_bytes + 1024 > buffer_size) {
      buffer_size *= 2;
      char *new_buffer = realloc(buffer, buffer_size);
      if (!new_buffer) {
        perror("realloc");
        free(buffer);
        return -1;
      }
      buffer = new_buffer;
    }

    bytes_read = read(sockfd, buffer + total_bytes, 1024);
    if (bytes_read < 0) {
      perror("Error reading from socket");
      free(buffer);
      return -1;
    } else if (bytes_read == 0) {
      break;
    }

    total_bytes += bytes_read;

    headers_end = strstr(buffer, "\r\n\r\n");
    if (headers_end) {
      headers_done = 1;
    }
  }

  if (!headers_done) {
    fprintf(stderr,
            "Malformed HTTP request: headers not properly terminated.\n");
    free(buffer);
    return -1;
  }

  ssize_t header_length = headers_end + 4 - buffer;
  if (httprequest_parse_headers(req, buffer, header_length) < 0) {
    free(buffer);
    return -1;
  }

  const char *content_length_str =
      httprequest_get_header(req, "Content-Length");
  if (!content_length_str) {
    req->payload = NULL;
    free(buffer);
    return total_bytes;
  }

  char *endptr = NULL;
  long content_length = strtol(content_length_str, &endptr, 10);
  if (endptr == content_length_str || *endptr != '\0' || content_length < 0) {
    req->payload = NULL;
    free(buffer);
    return total_bytes;
  }

  if (content_length > 0) {
    req->payload = (char *)malloc(content_length);
    if (!req->payload) {
      perror("malloc");
      free(buffer);
      return -1;
    }

    ssize_t payload_bytes_read = total_bytes - header_length;
    memcpy(req->payload, buffer + header_length, payload_bytes_read);

    while (payload_bytes_read < content_length) {
      ssize_t bytes = read(sockfd, req->payload + payload_bytes_read,
                           content_length - payload_bytes_read);
      if (bytes < 0) {
        perror("Error reading payload from socket");
        free(req->payload);
        req->payload = NULL;
        free(buffer);
        return -1;
      }
      payload_bytes_read += bytes;
    }
  }

  free(buffer);
  return total_bytes;
}

const char *httprequest_get_action(HTTPRequest *req) {
  if (req) {
    return req->action;
  }
  return NULL;
}

const char *httprequest_get_path(HTTPRequest *req) {
  if (req) {
    return req->path;
  }
  return NULL;
}

const char *httprequest_get_header(HTTPRequest *req, const char *key) {
  if (!req || !key) return NULL;

  for (int i = 0; i < req->header_count; i++) {
    if (strcasecmp(req->headers[i].key, key) == 0) {
      return req->headers[i].value;
    }
  }
  return NULL;
}

void httprequest_destroy(HTTPRequest *req) {
  if (!req) return;

  free(req->action);
  free(req->path);
  free(req->version);
  free(req->payload);
  for (int i = 0; i < req->header_count; i++) {
    free(req->headers[i].key);
    free(req->headers[i].value);
  }

  free(req->headers);
}