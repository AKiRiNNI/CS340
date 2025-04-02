#pragma once
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *key;
  const char *value;
} Header;

typedef struct HTTPRequest {
  char *action;   // HTTP action verb (e.g., "GET")
  char *path;     // Path (URI) (e.g., "/")
  char *version;  // HTTP version (e.g., "HTTP/1.1")
  char *payload;  // Payload (can be used for POST/PUT requests)

  Header *headers;   // Dynamic array of headers
  int header_count;  // Number of headers
} HTTPRequest;

ssize_t httprequest_read(HTTPRequest *req, int sockfd);
ssize_t httprequest_parse_headers(HTTPRequest *req, char *buffer,
                                  ssize_t buffer_len);
const char *httprequest_get_action(HTTPRequest *req);
const char *httprequest_get_header(HTTPRequest *req, const char *key);
const char *httprequest_get_path(HTTPRequest *req);
void httprequest_destroy(HTTPRequest *req);

#ifdef __cplusplus
}
#endif