
#include "metrics.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

const uint32_t UPDATE_INTERVAL_SECONDS = 1;

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

class AgentConn {
  Socket client_socket = -1;
  vector<MetricsData> metrics_history;
  bool is_connected = false;

  MetricsData request_metrics();

public:
  string address = "";
  uint16_t port = 0;

  AgentConn(const std::string &addr, uint16_t port)
      : address(addr), port(port) {}
  bool connect();
  MetricsData update_metrics();
  void close();
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

MetricsData AgentConn::update_metrics() {
  MetricsData data = request_metrics();
  metrics_history.push_back(data);

  return data;
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

    for (AgentConn &agent : agents) {
      if (!agent.connect()) {
        cerr << "Manager: Failed to connect to agent " << agent.address << ":"
             << agent.port << endl;
      }
    }

    while (true) {
      // Request metrics from all agents
      for (AgentConn &agent : agents) {
        MetricsData data = agent.update_metrics();
        cout << "Metrics from agent " << agent.address << ":" << agent.port
             << " - CPU: " << data.cpu_usage
             << "%, Memory: " << data.memory_usage
             << "%, Uptime: " << data.uptime_seconds
             << "s, TX Rate: " << data.network_usage.tx_rate
             << " Mbps, RX Rate: " << data.network_usage.rx_rate << " Mbps"
             << endl;
      }

      // Wait some time before next request
      this_thread::sleep_for(chrono::seconds(UPDATE_INTERVAL_SECONDS));
    }
  }
};

int main() {
  vector<string> agent_addresses = {
      "127.0.0.1:54001",
  };

  Manager manager(agent_addresses);
  manager.run();
}
