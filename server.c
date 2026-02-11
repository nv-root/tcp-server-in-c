#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 4000
#define BUFFER_SIZE 1024

int main() {

  int server_fd, client_fd;
  char buffer[BUFFER_SIZE];

  // structures for describing the internet address
  // like port, ip family, ip address
  struct sockaddr_in server_addr, client_addr;

  socklen_t client_len = sizeof(client_addr);

  // creates a socket and returns an FD
  // AF_INET - address family ipv4/ipv6
  // SOCK_STREAM - stream type of socket
  // here, 0 lets the system choose the protocol like TCP for streams
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // socket option to reuse the existing socket if the system has to reconnect
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // clear the structure from garbage by setting to 0
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET; // ipv4
  server_addr.sin_addr.s_addr =
      INADDR_ANY; // accept connections from any interface

  // converts the port number from host byte order to network byte order
  // basically chaning the endianness
  server_addr.sin_port = htons(PORT);

  // bind the socket to the address
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // 10 -> backlog queue size (maximum pending connections to be in the queue)
  if (listen(server_fd, 10) < 0) {
    perror("listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d...\n", PORT);

  while (1) {
    // blocks until a client connection
    // fills the client address struct
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      perror("accept failed");
      continue;
    }

    printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    // read data from the client
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read > 0) {
      printf("Received: %s\n", buffer);

      const char *response = "Message received\n";
      send(client_fd, response, strlen(response), 0);
    }

    close(client_fd);
  }

  close(server_fd);
  return 0;
}
