#include "server.hpp"

#include <cstring>
#include <iostream>

using namespace std;

WorkerPool::WorkerPool(size_t num_workers) : shutdown_flag(false) {
  for (size_t i = 0; i < num_workers; ++i) {
    workers.emplace_back(&WorkerPool::worker_thread, this);
  }
}

WorkerPool::~WorkerPool() { shutdown(); }

void WorkerPool::enqueue(Task task) {
  {
    unique_lock<mutex> lock(queue_mutex);
    if (shutdown_flag) {
      throw runtime_error("Cannot enqueue task after shutdown");
    }
    task_queue.push(task);
  }
  // Notify a waiting worker thread
  cv.notify_one();
}

void WorkerPool::shutdown() {
  {
    unique_lock<mutex> lock(queue_mutex);
    shutdown_flag = true;
  }
  // Notify all worker threads to wake up
  cv.notify_all();

  // Wait for all worker threads to finish
  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void WorkerPool::worker_thread() {
  while (true) {
    Task task;
    {
      unique_lock<mutex> lock(queue_mutex);
      // Wait for a task or shutdown signal
      cv.wait(lock, [this] { return !task_queue.empty() || shutdown_flag; });

      // If shutdown and queue is empty, exit
      if (shutdown_flag && task_queue.empty()) {
        break;
      }

      // If queue is not empty, get a task
      if (!task_queue.empty()) {
        task = task_queue.front();
        task_queue.pop();
      } else {
        continue; // Spurious wakeup
      }
    }

    // Execute the task outside the lock
    if (task) {
      task();
    }
  }
}

SocketServer::SocketServer(int port, size_t num_workers)
    : server_socket(-1), port(port), running(false) {
  worker_pool = make_unique<WorkerPool>(num_workers);
  memset(&server_addr, 0, sizeof(server_addr));
}

SocketServer::~SocketServer() { stop(); }

bool SocketServer::initialize() {
  // Create socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1) {
    cerr << "Error: Failed to create socket" << endl;
    return false;
  }

  // Allow socket reuse to avoid "Address already in use" error
  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    cerr << "Error: Failed to set socket options" << endl;
    close(server_socket);
    return false;
  }

  // Setup server address structure
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
  server_addr.sin_port = htons(port);

  // Bind socket to port
  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    cerr << "Error: Failed to bind socket to port " << port << endl;
    close(server_socket);
    return false;
  }

  // Listen for incoming connections (backlog of 5)
  if (listen(server_socket, 5) < 0) {
    cerr << "Error: Failed to listen on socket" << endl;
    close(server_socket);
    return false;
  }

  cout << "Server initialized and listening on port " << port << endl;
  return true;
}

void SocketServer::start() {
  if (server_socket == -1) {
    cerr << "Error: Server not initialized" << endl;
    return;
  }

  running = true;
  cout << "Server started, waiting for connections..." << endl;

  while (running) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Accept incoming connection
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                               &client_addr_len);

    if (client_socket < 0) {
      if (running) {
        cerr << "Error: Failed to accept connection" << endl;
      }
      continue;
    }

    // Get client IP and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    cout << "New connection from " << client_ip << ":" << client_port << endl;

    // Enqueue task to handle this client
    try {
      worker_pool->enqueue([this, client_socket, client_addr]() {
        this->handle_client(client_socket, client_addr);
      });
    } catch (const exception &e) {
      cerr << "Error: " << e.what() << endl;
      close(client_socket);
    }
  }
}

void SocketServer::stop() {
  running = false;

  if (server_socket != -1) {
    close(server_socket);
    server_socket = -1;
  }

  if (worker_pool) {
    worker_pool->shutdown();
  }

  cout << "Server stopped" << endl;
}

string SocketServer::read_request(int client_socket) {
  const int BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE] = {0};

  ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

  if (bytes_read < 0) {
    cerr << "Error: Failed to receive data from client" << endl;
    return "";
  } else if (bytes_read == 0) {
    cout << "Client closed connection" << endl;
    return "";
  }

  return string(buffer, bytes_read);
}

void SocketServer::send_response(int client_socket, const string &message) {
  ssize_t bytes_sent =
      send(client_socket, message.c_str(), message.length(), 0);

  if (bytes_sent < 0) {
    cerr << "Error: Failed to send data to client" << endl;
  } else {
    cout << "Sent " << bytes_sent << " bytes to client" << endl;
  }
}
