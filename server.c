#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 4000
#define BUFFER_SIZE 1024

volatile sig_atomic_t running = 1;

void handle_client(int client_fd, struct sockaddr_in client_addr);

// reap all finished children to prevent zombies
void sigchld_handler(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

void sigint_handler(int sig) {
  (void)sig;
  running = 0;
}

int main() {

  int server_fd, client_fd;

  // structures for describing the internet address
  // like port, ip family, ip address
  struct sockaddr_in server_addr, client_addr;

  socklen_t client_len = sizeof(client_addr);

  struct sigaction sa_chld, sa_int;

  // ---------- SIGCHLD ----------
  sa_chld.sa_handler = sigchld_handler;
  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sa_chld, NULL);

  // ---------- SIGINT -----------
  sa_int.sa_handler = sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  if (sigaction(SIGINT, &sa_int, NULL) < 0) {
    perror("sigaction SIGINT");
    exit(EXIT_FAILURE);
  }

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

  while (running) {
    // blocks until a client connection
    // fills the client address struct
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      if (errno == EINTR)
        continue;
      perror("accept failed");
      continue;
    }

    printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork failed");
      close(client_fd);
    } else if (pid == 0) {
      close(server_fd);
      handle_client(client_fd, client_addr);
    } else {
      close(client_fd);
    }
  }

  printf("Shutdown");
  close(server_fd);
  return 0;
}

void handle_client(int client_fd, struct sockaddr_in client_addr) {
  char buffer[BUFFER_SIZE];
  char client_ip[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  printf("Child process handling client %s:%d\n", client_ip,
         ntohs(client_addr.sin_port));

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        printf("[%s:%d] -> connection closed\n", client_ip,
               ntohs(client_addr.sin_port));
      } else {
        perror("recv failed");
      }
      break;
    }

    buffer[bytes_read] = '\0';
    printf("[%s:%d]\n-> %s\n", client_ip, ntohs(client_addr.sin_port), buffer);
    const char *response = "Message received. Now f off\n";
    send(client_fd, response, strlen(response), 0);
  }

  printf("Client %s:%d disconnected\n", client_ip, ntohs(client_addr.sin_port));
  close(client_fd);
  exit(EXIT_SUCCESS);
}
