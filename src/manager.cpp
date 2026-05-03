
#include "metrics.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

const uint32_t UPDATE_INTERVAL_SECONDS = 1;

using namespace std;

using Socket = int;

pair<MetricsData, bool> parse_response(string response) {
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

  return {data, true};
}

struct RequestMetricsResult {
  bool success;
  MetricsData data;
};

class AgentConn {
  Socket client_socket = -1;
  vector<MetricsData> metrics_history;
  bool is_connected = false;

  RequestMetricsResult request_metrics();

public:
  string address = "";
  uint16_t port = 0;

  AgentConn(const std::string &addr, uint16_t port)
      : address(addr), port(port) {}
  bool connect();
  RequestMetricsResult update_metrics();
  void close();
  bool is_available() const { return is_connected; }
};

bool AgentConn::connect() {
  if (is_connected) {
    return true;
  }

  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    std::cerr << "Manager: Failed to create socket for agent " << address << ":"
              << port << std::endl;
    return false;
  }

  timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 500000; // 500 ms

  // Timeout para receber
  setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
             sizeof(timeout));

  // Timeout para enviar
  setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout,
             sizeof(timeout));

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

  is_connected = true;
  return true;
}

void AgentConn::close() {
  if (is_connected) {
    ::close(client_socket);
    is_connected = false;
  }
}

RequestMetricsResult AgentConn::request_metrics() {
  const string request = "GET_METRICS";
  ssize_t bytes_sent =
      send(client_socket, request.c_str(), request.length(), 0);

  if (bytes_sent < 0) {
    std::cerr << "Manager: Failed to send request to agent " << address << ":"
              << port << std::endl;
    return {false, {}};
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
    return {false, {}};
  }

  string response(buffer, bytes_read);

  auto result = parse_response(response);

  if (!result.second) {
    return {false, {}};
  }

  auto data = result.first;

  return {true, data};
}

RequestMetricsResult AgentConn::update_metrics() {
  RequestMetricsResult result = request_metrics();

  if (!result.success) {
    cerr << "Manager: Failed to get metrics from agent " << address << ":"
         << port << endl;

    close();
    return {false, {}};
  }

  metrics_history.push_back(result.data);

  return {true, result.data};
}

class Manager {
  vector<AgentConn> agents;

public:
  Manager(vector<string> agent_addresses) {
    for (string addr : agent_addresses) {
      vector<string> parts = split(addr, ':');
      if (parts.size() != 2) {
        cerr << "Manager: Invalid agent address format: " << addr << endl;
        continue;
      }

      string ip = parts[0];
      uint16_t port = stoi(parts[1]);

      agents.emplace_back(ip, port);
    }
  }

  void run() {
    while (true) {
      // Request metrics from all agents
      for (AgentConn &agent : agents) {
        if (!agent.is_available()) {
          bool result = agent.connect();
          if (!result) {
            cerr << "Manager: Failed to reconnect to agent " << agent.address
                 << ":" << agent.port << endl;
            continue;
          }
        }

        RequestMetricsResult result = agent.update_metrics();

        if (result.success) {
          MetricsData data = result.data;
          cout << "Metrics from agent " << agent.address << ":" << agent.port
               << " - CPU: " << data.cpu_usage
               << "%, Memory: " << data.memory_usage
               << "%, Uptime: " << data.uptime_seconds
               << "s, TX Rate: " << data.network_usage.tx_rate / 1024
               << " Kbps, RX Rate: " << data.network_usage.rx_rate / 1024
               << " Kbps" << endl;
        } else {
          cout << "Agent invalid or disconnected: " << agent.address << ":"
               << agent.port << endl;
        }
      }

      // Wait some time before next request
      this_thread::sleep_for(chrono::seconds(UPDATE_INTERVAL_SECONDS));
    }
  }
};

int main(int argc, char *argv[]) {
  vector<string> agent_addresses;

  for (int i = 1; i < argc; i++) {
    agent_addresses.push_back(argv[i]);
  };

  Manager manager(agent_addresses);
  manager.run();
}
