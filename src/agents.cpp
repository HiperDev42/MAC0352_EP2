#include "metrics.hpp"
#include "server.hpp"

#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

#define AGENT_PORT 54001
#define NUM_WORKER_THREADS 2
#define METRIC_DELTA_TIME_MS 500

SocketServer *g_agent_server = nullptr;

static void signal_handler(int signum) {
  std::cout << "Agent: Caught signal " << signum << ", shutting down..."
            << std::endl;

  if (g_agent_server) {
    g_agent_server->stop();
  }

  std::exit(0);
}

class AgentServer : public SocketServer {
public:
  AgentServer(int port, std::string interface, int num_threads)
      : SocketServer(port, num_threads),
        metrics_collector(static_cast<double>(METRIC_DELTA_TIME_MS) / 1000.0) {
    metrics_collector.network_interface = interface;
  }

  void start() override {
    metrics_collector.start();
    SocketServer::start();
  }

  void stop() override {
    metrics_collector.stop();
    SocketServer::stop();
  }

  void handle_client(int client_socket, sockaddr_in client_addr) override {
    try {
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

      while (true) {
        // Keep this connection open and serve multiple requests.
        string request = read_request(client_socket);
        if (request.empty()) {
          break;
        }

        cout << "Agent: Received request from " << client_ip << ": " << request
             << endl;

        string response = process_request(request);

        cout << "Response to " << client_ip << ": " << response << endl;
        send_response(client_socket, response);
      }
    } catch (const exception &e) {
      cerr << "Agent: Error handling manager request: " << e.what() << endl;
    }

    close(client_socket);
  }

private:
  MetricsCollector metrics_collector;

  // Process incoming request from manager
  string process_request(const string &request) {
    (void)request;
    MetricsData metrics = metrics_collector.get_metrics();

    std::ostringstream response;
    response << std::fixed << std::setprecision(2);
    response << "uptime_seconds=" << metrics.uptime_seconds
             << ";cpu_usage=" << metrics.cpu_usage
             << ";memory_usage=" << metrics.memory_usage
             << ";tx_rate=" << metrics.network_usage.tx_rate
             << ";rx_rate=" << metrics.network_usage.rx_rate;

    return response.str();
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

int main(int argc, char *argv[]) {
  // Parse port from command-line arguments, default to AGENT_PORT
  int port = AGENT_PORT;

  if (argc > 1) {
    try {
      port = stoi(argv[1]);
    } catch (const exception &e) {
      cerr << "Agent: Invalid port number: " << argv[1] << endl;
      cerr << "Usage: " << argv[0] << " [port]" << endl;
      return 1;
    }
  }

  std::string interface = "wlan0";
  if (argc > 2) {
    std::string interface = argv[2];
  }

  // Create agent server instance
  AgentServer agent_server(port, interface, NUM_WORKER_THREADS);

  // Setup signal handlers for graceful shutdown
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  cout << "=== Agent Server ===" << endl;

  // Initialize the server
  if (!agent_server.initialize()) {
    cerr << "Agent: Failed to initialize server" << endl;
    return 1;
  }

  cout << "Agent: Ready to receive requests from manager" << endl;
  cout << "Agent: Listening on port " << port << endl;
  cout << "Agent: Press Ctrl+C to shutdown" << endl << endl;

  // Start accepting connections from manager
  agent_server.start();

  return 0;
}
