#pragma once

#include <arpa/inet.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

using Task = std::function<void()>;

class WorkerPool {
public:
  explicit WorkerPool(size_t num_workers);
  ~WorkerPool();

  void enqueue(Task task);

  void shutdown();

private:
  std::vector<std::thread> workers;
  std::queue<Task> task_queue;
  std::mutex queue_mutex;
  std::condition_variable cv;
  bool shutdown_flag;

  void worker_thread();
};

class SocketServer {
public:
  explicit SocketServer(int port, size_t num_workers);
  virtual ~SocketServer();

  bool initialize();
  virtual void start();
  virtual void stop();
  virtual void handle_client(int client_socket, sockaddr_in client_addr) = 0;

protected:
  int server_socket;
  int port;
  sockaddr_in server_addr;
  std::unique_ptr<WorkerPool> worker_pool;
  bool running;

  void send_response(int client_socket, const std::string &message);
  std::string read_request(int client_socket);
};
