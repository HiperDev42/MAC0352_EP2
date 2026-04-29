#include "server.hpp"

#include <csignal>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;

#define AGENT_PORT 54001
#define NUM_WORKER_THREADS 2

SocketServer *g_agent_server = nullptr;

struct CpuData {
  long user, nice, system, idle, iowait, irq, softirq, steal;
};

CpuData read_cpu_data() {
  std::ifstream stat_file("/proc/stat");
  std::string line;
  std::getline(stat_file, line);
  
  std::istringstream iss(line);
  std::string cpu;
  
  CpuData data;
  iss >> cpu
      >> data.user
      >> data.nice
      >> data.system
      >> data.idle
      >> data.iowait
      >> data.irq
      >> data.softirq
      >> data.steal;

  return data;
}

double get_cpu_usage() {
  CpuData cpu_data = read_cpu_data();
  

  return 0.0;
}

double get_memory_usage() {
  return 0.0;
}

// Signal handler for graceful shutdown
void signal_handler(int signum) {
  cout << "\nAgent: Received signal " << signum << ", shutting down..." << endl;
  if (g_agent_server) {
    g_agent_server->stop();
  }
}

// Override handle_client to process manager requests
class AgentServer : public SocketServer {
public:
  using SocketServer::SocketServer;

  void handle_client(int client_socket, sockaddr_in client_addr) override {
    try {
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

      // Read request from manager
      string request = read_request(client_socket);

      if (!request.empty()) {
        cout << "Agent: Received request from " << client_ip << ": " << request
             << endl;

        // Process the request (example: echo it back)
        string response = process_request(request);
        send_response(client_socket, response);
      }
    } catch (const exception &e) {
      cerr << "Agent: Error handling manager request: " << e.what() << endl;
    }

    close(client_socket);
  }

private:
  // Process incoming request from manager
  string process_request(const string &request) {
    // TODO: Implement actual agent logic here
    // For now, just echo back the request with "Agent processed:" prefix
    cout << "Agent: Processing request..." << endl;
    return "Agent processed: " + request;
  }

  // Helper to read request (inherited but made accessible)
  string read_request(int client_socket) {
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE] = {0};

    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_read < 0) {
      cerr << "Agent: Failed to receive data from manager" << endl;
      return "";
    } else if (bytes_read == 0) {
      cout << "Agent: Manager closed connection" << endl;
      return "";
    }

    return string(buffer, bytes_read);
  }

  // Helper to send response
  void send_response(int client_socket, const string &message) {
    ssize_t bytes_sent =
        send(client_socket, message.c_str(), message.length(), 0);

    if (bytes_sent < 0) {
      cerr << "Agent: Failed to send response to manager" << endl;
    } else {
      cout << "Agent: Sent " << bytes_sent << " bytes to manager" << endl;
    }
  }
};

int main() {
  while (true) {
    cout << "CPU Usage: " << get_cpu_usage() << "%" << endl;
    cout << "Memory Usage: " << get_memory_usage() << "%" << endl;
    this_thread::sleep_for(chrono::seconds(1));
  }

  // Create agent server instance
  // AgentServer agent_server(AGENT_PORT, NUM_WORKER_THREADS);
  // g_agent_server = &agent_server;

  // // Setup signal handlers for graceful shutdown
  // signal(SIGINT, signal_handler);
  // signal(SIGTERM, signal_handler);

  // cout << "=== Agent Server ===" << endl;

  // // Initialize the server
  // if (!agent_server.initialize()) {
  //   cerr << "Agent: Failed to initialize server" << endl;
  //   return 1;
  // }

  // cout << "Agent: Ready to receive requests from manager" << endl;
  // cout << "Agent: Listening on port " << AGENT_PORT << endl;
  // cout << "Agent: Press Ctrl+C to shutdown" << endl << endl;

  // // Start accepting connections from manager
  // agent_server.start();

  // return 0;
}
