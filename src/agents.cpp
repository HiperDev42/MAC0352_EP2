#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#define ADDRESS "0.0.0.0"
#define PORT 54000

int server_socket;

void init_socket();
void stop_socket();

int main() {
  cout << "Hello, World!" << endl;
  init_socket();

  while (true) {
    cout << "Waiting for a client to connect..." << endl;
    sockaddr_in client;
    socklen_t client_size = sizeof(client);
    int client_socket =
        accept(server_socket, (sockaddr *)&client, &client_size);
    if (client_socket == -1) {
      cerr << "Failed to accept client connection" << endl;
    }
  }

  stop_socket();
  return 0;
}

void init_socket() {
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1) {
    cerr << "Failed to create socket" << endl;
    exit(1);
  }

  sockaddr_in hint;
  hint.sin_family = AF_INET;
  hint.sin_port = htons(PORT);
  hint.sin_addr.s_addr = INADDR_ANY;
  inet_pton(AF_INET, ADDRESS, &hint.sin_addr);

  if (bind(server_socket, (sockaddr *)&hint, sizeof(hint)) == -1) {
    cerr << "Failed to bind socket" << endl;
    exit(2);
  }

  if (listen(server_socket, SOMAXCONN) == -1) {
    cerr << "Failed to listen on socket" << endl;
    exit(3);
  }

  return;
}

void stop_socket() {
  close(server_socket);
  return;
}