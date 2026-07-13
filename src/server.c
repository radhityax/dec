#include "server.h"
#include "builder.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 1204
#define BUFFER_SIZE 4096

static const char *get_mime_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";

  if (strcmp(ext, ".html") == 0)
    return "text/html";
  if (strcmp(ext, ".css") == 0)
    return "text/css";
  if (strcmp(ext, ".js") == 0)
    return "application/javascript";
  if (strcmp(ext, ".png") == 0)
    return "image/png";
  if (strcmp(ext, ".jpg") == 0)
    return "image/jpeg";

  return "application/octet-stream";
}

static void send_response(int client_fd, int status, const char *status_text,
                          const char *content_type, const char *body,
                          size_t body_len) {

  char header[BUFFER_SIZE];
  int header_text = snprintf(header, sizeof(header),
                             "HTTP/1.1 %d %s\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             status, status_text, content_type, body_len);
  write(client_fd, header, header_text);
  if (body && body_len > 0) {
    write(client_fd, body, body_len);
  }
}

static char *read_file(const char *path, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size < 0) {
    fclose(f);
    return NULL;
  }

  char *buffer = malloc(size);
  if (!buffer) {
    fclose(f);
    return NULL;
  }

  size_t read = fread(buffer, 1, size, f);
  fclose(f);

  if (read != (size_t)size) {
    free(buffer);
    return NULL;
  }

  *out_size = size;
  return buffer;
}

static void handle_request(int client_fd) {
  char buffer[BUFFER_SIZE];

  ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0)
    return;

  buffer[bytes_read] = '\0';

  char method[16], path[1024];
  if (sscanf(buffer, "%15s %1023s", method, path) != 2) {
    send_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request",
                  11);
    return;
  }

  if (strcmp(method, "GET") != 0) {
    send_response(client_fd, 405, "Method Not Allowed", "text/plain",
                  "Method Not Allowed", 18);
    return;
  }

  if (strcmp(path, "/") == 0) {
    strcpy(path, "/index.html");
  }

  char file_path[2048];
  snprintf(file_path, sizeof(file_path), "public%s", path);

  size_t file_size;
  char *file_content = read_file(file_path, &file_size);

  if (!file_content) {
    send_response(client_fd, 404, "Not Found", "text/plain", "404 Not Found",
                  13);
    return;
  }

  const char *mime = get_mime_type(file_path);
  send_response(client_fd, 200, "OK", mime, file_content, file_size);

  free(file_content);
}

void serve_site(void) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket failed");
    return;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    return;
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen failed");
    close(server_fd);
    return;
  }

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      perror("accept failed");
      continue;
    }

    handle_request(client_fd);

    close(client_fd);
  }

  close(server_fd);
  return;
}

static void *watcher_loop(void *arg) {
  (void)arg;

  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
    return NULL;
  }

  inotify_add_watch(fd, "content", IN_MODIFY | IN_CREATE | IN_DELETE);
  inotify_add_watch(fd, "layouts", IN_MODIFY | IN_CREATE | IN_DELETE);
  inotify_add_watch(fd, "media", IN_MODIFY | IN_CREATE | IN_DELETE);

  char buf[4096];

  while (1) {
    read(fd, buf, sizeof(buf));
    sleep(1);

    printf("\nchanges detected, rebuilding..");
    build_site(NULL);
    build_media(NULL);
    printf("rebuilt.\n");
  }
}

void serve_dev(void) {
  build_site(NULL);
  build_media(NULL);

  pthread_t tid;
  pthread_create(&tid, NULL, watcher_loop, NULL);

  serve_site();
}
