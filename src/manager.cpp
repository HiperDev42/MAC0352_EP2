
#include "metrics.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

using namespace std;

using Socket = int;

MetricsData parse_response(string response) {
  vector<string> parts = split(response, ';');
  unordered_map<string, string> kv_map;

  for (string p : parts) {
    vector<string> kv = split(p, '=');
    if (kv.size() != 2) {
      cerr << "Manager: Invalid response part: " << p << endl;
      continue;
    }

    string key = kv[0];
    string value = kv[1];

    kv_map[key] = value;
  }

  MetricsData data{};

  data.cpu_usage = stof(kv_map["cpu_usage"]);
  data.memory_usage = stof(kv_map["memory_usage"]);
  data.uptime_seconds = stof(kv_map["uptime_seconds"]);
  data.network_usage.tx_rate = stof(kv_map["tx_rate"]);
  data.network_usage.rx_rate = stof(kv_map["rx_rate"]);

  return data;
}

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

  MetricsData data = parse_response(response);
  return data;
}

int main() {
  vector<string> agent_addresses = {
      "127.0.0.1:54001",
  };

  for (string addr : agent_addresses) {
    vector<string> parts = split(addr, ':');
    if (parts.size() != 2) {
      cerr << "Manager: Invalid agent address format: " << addr << endl;
      continue;
    }

    string ip = parts[0];
    uint16_t port = stoi(parts[1]);

    AgentConn agent(ip, port);
    if (!agent.connect()) {
      cerr << "Manager: Failed to connect to agent at " << ip << ":" << port
           << endl;
      continue;
    }

    MetricsData data = agent.request_metrics();
    cout << "Metrics from agent " << ip << ":" << port << " - "
         << "CPU: " << data.cpu_usage << "%, \n"
         << "Memory: " << data.memory_usage << "%, \n"
         << "Uptime: " << data.uptime_seconds << "s, \n"
         << "TX Rate: " << data.network_usage.tx_rate << " Mbps, \n"
         << "RX Rate: " << data.network_usage.rx_rate << " Mbps" << endl;
  }
}
