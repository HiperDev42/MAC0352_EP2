#include "metrics.hpp"

#include <chrono>
#include "metrics.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/sysinfo.h>
#include <unordered_map>

using namespace std::chrono_literals;

// Read a snapshot of /proc/stat (first "cpu" line)
CpuData read_cpu_data() {
  std::ifstream stat_file("/proc/stat");
  std::string line;
  if (!std::getline(stat_file, line)) {
    return CpuData{0, 0, 0, 0, 0, 0, 0, 0};
  }

  std::istringstream iss(line);
  std::string cpu;

  CpuData data{};
  iss >> cpu >> data.user >> data.nice >> data.system >> data.idle >> data.iowait >> data.irq >> data.softirq >> data.steal;

  return data;
}

double get_cpu_usage() {
  CpuData a = read_cpu_data();
  std::this_thread::sleep_for(100ms);
  CpuData b = read_cpu_data();

  long idleA = a.idle + a.iowait;
  long idleB = b.idle + b.iowait;

  long totalA = a.user + a.nice + a.system + a.idle + a.iowait + a.irq + a.softirq + a.steal;
  long totalB = b.user + b.nice + b.system + b.idle + b.iowait + b.irq + b.softirq + b.steal;

  long totalDiff = totalB - totalA;
  long idleDiff = idleB - idleA;

  if (totalDiff <= 0) return 0.0;
  return 100.0 * (totalDiff - idleDiff) / static_cast<double>(totalDiff);
}

static std::unordered_map<std::string, long> read_meminfo_map() {
  std::ifstream file("/proc/meminfo");
  std::string key;
  long value;
  std::string unit;

  std::unordered_map<std::string, long> mem;
  while (file >> key >> value >> unit) {
    if (!key.empty() && key.back() == ':') key.pop_back();
    mem[key] = value;
  }
  return mem;
}

MemoryInfo get_memory_info() {
  auto mem = read_meminfo_map();
  MemoryInfo info{};
  info.totalKB = mem["MemTotal"];
  info.availableKB = mem["MemAvailable"];
  info.usedKB = info.totalKB - info.availableKB;
  info.usedPercent = (info.totalKB > 0) ? (100.0 * info.usedKB / info.totalKB) : 0.0;
  return info;
}

double get_memory_usage() {
  auto info = get_memory_info();
  return info.usedPercent;
}

long read_net_tx_bytes(const std::string &interface) {
  std::ifstream file("/sys/class/net/" + interface + "/statistics/tx_bytes");
  long tx_bytes = 0;
  file >> tx_bytes;
  return tx_bytes;
}

long read_net_rx_bytes(const std::string &interface) {
  std::ifstream file("/sys/class/net/" + interface + "/statistics/rx_bytes");
  long rx_bytes = 0;
  file >> rx_bytes;
  return rx_bytes;
}

NetStats read_network_stats(const std::string &interface) {
  NetStats stats;
  stats.tx_bytes = read_net_tx_bytes(interface);
  stats.rx_bytes = read_net_rx_bytes(interface);
  return stats;
}

// Return delta (bytes) over a short sample interval (internal 100ms)
NetStats get_network_stats() {
  const std::string interface = "eth0";
  NetStats a = read_network_stats(interface);
  std::this_thread::sleep_for(100ms);
  NetStats b = read_network_stats(interface);
  NetStats delta;
  delta.tx_bytes = b.tx_bytes - a.tx_bytes;
  delta.rx_bytes = b.rx_bytes - a.rx_bytes;
  return delta;
}

// MetricsCollector implementation
MetricsCollector::MetricsCollector(double dt_seconds_, const std::string &network_interface_)
    : worker_thread(), running(false), metrics(), dt_seconds(dt_seconds_), network_interface(network_interface_) {}

MetricsCollector::~MetricsCollector() { stop(); }

void MetricsCollector::start() {
  bool expected = false;
  if (!running.compare_exchange_strong(expected, true)) return; // already running

  worker_thread = std::thread([this]() { this->run(); });
}

void MetricsCollector::stop() {
  bool expected = true;
  if (!running.compare_exchange_strong(expected, false)) return; // not running

  if (worker_thread.joinable()) worker_thread.join();
}

MetricsData MetricsCollector::get_metrics() {
  std::lock_guard<std::mutex> lk(data_lock);
  return metrics;
}

void MetricsCollector::run() {
  using clock = std::chrono::steady_clock;
  while (running.load()) {
    auto start = clock::now();

    double cpu = get_cpu_usage();
    MemoryInfo mem = get_memory_info();

    // Network: sample quickly to estimate rate
    NetStats a = read_network_stats(network_interface);
    std::this_thread::sleep_for(100ms);
    NetStats b = read_network_stats(network_interface);
    double deltaSec = 0.1; // 100ms
    NetworkUsage netU{};
    netU.tx_rate = (b.tx_bytes - a.tx_bytes) / deltaSec;
    netU.rx_rate = (b.rx_bytes - a.rx_bytes) / deltaSec;

    {
      std::lock_guard<std::mutex> lk(data_lock);
      metrics.cpu_usage = cpu;
      metrics.memory_info = mem;
      metrics.network_usage = netU;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(clock::now() - start).count();
    double to_sleep = dt_seconds - elapsed;
    if (to_sleep > 0) std::this_thread::sleep_for(std::chrono::duration<double>(to_sleep));
  }
}