
#include "metrics.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>

using namespace std;

using Socket = int;

struct AgentConn {
  string address = "";
  uint16_t port = 0;

  Socket client_socket = -1;

public:
  AgentConn(const std::string &addr, uint16_t port)
      : address(addr), port(port) {}
  bool connect();
  MetricsData request_metrics();
};

bool AgentConn::connect() {
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    std::cerr << "Manager: Failed to create socket for agent " << address << ":"
              << port << std::endl;
    return false;
  }

  sockaddr_in agent_addr{};
  agent_addr.sin_family = AF_INET;
  agent_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, address.c_str(), &agent_addr.sin_addr) <= 0) {
    std::cerr << "Manager: Invalid agent address " << address << std::endl;
    return false;
  }

  if (::connect(client_socket, (sockaddr *)&agent_addr, sizeof(agent_addr)) <
      0) {
    std::cerr << "Manager: Failed to connect to agent " << address << ":"
              << port << std::endl;
    return false;
  }

  return true;
}

MetricsData AgentConn::request_metrics() {
  const string request = "GET_METRICS";
  ssize_t bytes_sent =
      send(client_socket, request.c_str(), request.length(), 0);

  if (bytes_sent < 0) {
    std::cerr << "Manager: Failed to send request to agent " << address << ":"
              << port << std::endl;
    return {};
  }

  const int BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE] = {0};

  ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
  if (bytes_read < 0) {
    std::cerr << "Manager: Failed to receive response from agent " << address
              << ":" << port << std::endl;
    return {};
  } else if (bytes_read == 0) {
    std::cerr << "Manager: Agent " << address << ":" << port
              << " closed connection" << std::endl;
    return {};
  }

  string response(buffer, bytes_read);
}

int main() { return 0; }
