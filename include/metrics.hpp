#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

struct MemoryInfo {
  long totalKB;
  long availableKB;
  long usedKB;
  double usedPercent;
};

struct NetStats {
  long tx_bytes;
  long rx_bytes;
};

// Cpu snapshot used for computing CPU usage deltas
struct CpuData {
  long user, nice, system, idle, iowait, irq, softirq, steal;
};

// Helper functions (implemented in src/metrics.cpp)
double get_cpu_usage();
MemoryInfo get_memory_info();
double get_memory_usage();
NetStats get_network_stats();

struct NetworkUsage {
  double tx_rate; // in bytes per second
  double rx_rate; // in bytes per second
};

struct MetricsData {
  double cpu_usage;
  MemoryInfo memory_info;
  NetworkUsage network_usage;
};

class MetricsCollector {
 public:
  MetricsCollector(double dt_seconds = 1.0, const std::string &network_interface = "eth0");
  ~MetricsCollector();

  // Start the background collector thread. Safe to call multiple times.
  void start();

  // Stop the background thread and join.
  void stop();

  // Return the most recently collected metrics (thread-safe copy).
  MetricsData get_metrics();

 private:
  void run();

  std::thread worker_thread;
  std::atomic<bool> running{false};
  std::mutex data_lock;

  MetricsData metrics{};
  double dt_seconds;
  std::string network_interface;
};

