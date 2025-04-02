#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

void *client_thread(void *vptr) {
  int fd = (int)(ssize_t)vptr;
  HTTPRequest req;

  if (httprequest_read(&req, fd) < 0) {
    fprintf(stderr, "Failed to read HTTP request\n");
    close(fd);
    return NULL;
  }

  const char *path = httprequest_get_path(&req);

  if (strcmp(path, "/") == 0) {
    path = "/index.html";
  }

  char file_path[1024];
  snprintf(file_path, sizeof(file_path), "static%s", path);
  FILE *file = fopen(file_path, "rb");

  if (!file) {
    const char *not_found_response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 19\r\n"
        "\r\n"
        "404 Not Found\n";
    write(fd, not_found_response, strlen(not_found_response));
  } else {
    const char *content_type;
    if (strstr(file_path, ".png")) {
      content_type = "image/png";
    } else if (strstr(file_path, ".html")) {
      content_type = "text/html";
    } else {
      content_type = "application/octet-stream";
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *file_content = (char *)malloc(file_size);
    fread(file_content, 1, file_size, file);

    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             content_type, file_size);
    write(fd, header, strlen(header));
    write(fd, file_content, file_size);

    free(file_content);
    fclose(file);
  }

  close(fd);
  httprequest_destroy(&req);
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    return 1;
  }
  int port = atoi(argv[1]);
  printf(
      "Binding to port %d. Visit http://localhost:%d/ to interact with your "
      "server!\n",
      port, port);

  // socket:
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  // bind:
  struct sockaddr_in server_addr, client_address;
  memset(&server_addr, 0x00, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));

  // listen:
  listen(sockfd, 10);

  // accept:
  socklen_t client_addr_len;
  while (1) {
    client_addr_len = sizeof(struct sockaddr_in);
    int fd =
        accept(sockfd, (struct sockaddr *)&client_address, &client_addr_len);
    printf("Client connected (fd=%d)\n", fd);

    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, (void *)(ssize_t)fd);
    pthread_detach(tid);
  }

  return 0;
}