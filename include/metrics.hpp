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

struct InstantMetrics {
  double uptime_seconds;
  CpuData cpu;
  MemoryInfo memory_info;
  NetStats network_stats;
};

struct NetworkUsage {
  double tx_rate; // in bytes per second
  double rx_rate; // in bytes per second
};

struct MetricsData {
  double uptime_seconds;
  double cpu_usage;
  double memory_usage;
  NetworkUsage network_usage;
};

class MetricsCollector {
public:
  MetricsCollector(double dt_seconds = 1.0,
                   const std::string &network_interface = "eth0");
  ~MetricsCollector();

  // Start the background collector thread. Safe to call multiple times.
  void start();

  // Stop the background thread and join.
  void stop();

  // Return the most recently collected metrics (thread-safe copy).
  MetricsData get_metrics();

private:
  void run();
  InstantMetrics collect_metrics();
  void update_metrics();

  std::thread worker_thread;
  std::atomic<bool> running{false};
  std::mutex data_lock;

  MetricsData metrics{};
  InstantMetrics last_metrics;
  double dt_seconds;
  std::string network_interface;
};
