#include "metrics.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

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
  iss >> cpu >> data.user >> data.nice >> data.system >> data.idle >>
      data.iowait >> data.irq >> data.softirq >> data.steal;

  return data;
}

double calc_cpu_usage(CpuData a, CpuData b) {
  long idleA = a.idle + a.iowait;
  long idleB = b.idle + b.iowait;

  long totalA = a.user + a.nice + a.system + a.idle + a.iowait + a.irq +
                a.softirq + a.steal;
  long totalB = b.user + b.nice + b.system + b.idle + b.iowait + b.irq +
                b.softirq + b.steal;

  long totalDiff = totalB - totalA;
  long idleDiff = idleB - idleA;

  if (totalDiff <= 0)
    return 0.0;
  return 100.0 * (totalDiff - idleDiff) / static_cast<double>(totalDiff);
}

static std::unordered_map<std::string, long> read_meminfo_map() {
  std::ifstream file("/proc/meminfo");
  std::string key;
  long value;
  std::string unit;

  std::unordered_map<std::string, long> mem;
  while (file >> key >> value >> unit) {
    if (!key.empty() && key.back() == ':')
      key.pop_back();
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
  info.usedPercent =
      (info.totalKB > 0) ? (100.0 * info.usedKB / info.totalKB) : 0.0;
  return info;
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

double read_uptime_seconds() {
  std::ifstream file("/proc/uptime");
  double uptime_seconds = 0.0;
  file >> uptime_seconds;
  return uptime_seconds;
}

// MetricsCollector implementation
MetricsCollector::MetricsCollector(double dt_seconds_,
                                   const std::string &network_interface_)
    : worker_thread(), running(false), metrics(), dt_seconds(dt_seconds_),
      network_interface(network_interface_) {}

MetricsCollector::~MetricsCollector() { stop(); }

void MetricsCollector::start() {
  bool expected = false;
  if (!running.compare_exchange_strong(expected, true))
    return; // already running

  worker_thread = std::thread([this]() { this->run(); });
}

void MetricsCollector::stop() {
  bool expected = true;
  if (!running.compare_exchange_strong(expected, false))
    return; // not running

  if (worker_thread.joinable())
    worker_thread.join();
}

MetricsData MetricsCollector::get_metrics() {
  std::lock_guard<std::mutex> lk(data_lock);
  return metrics;
}

void MetricsCollector::run() {
  if (dt_seconds <= 0.0) {
    dt_seconds = 0.5;
  }

  last_metrics = collect_metrics();

  while (running.load()) {
    std::this_thread::sleep_for(std::chrono::duration<double>(dt_seconds));
    if (!running.load()) {
      break;
    }
    update_metrics();

    std::cout << "Metrics: CPU: " << metrics.cpu_usage << "%; "
              << "Memory: " << metrics.memory_usage << "%; "
              << "Network: tx " << metrics.network_usage.tx_rate << " rx "
              << metrics.network_usage.rx_rate << std::endl;
  }
}

InstantMetrics MetricsCollector::collect_metrics() {
  double uptime_seconds = read_uptime_seconds();
  CpuData cpu_data = read_cpu_data();
  MemoryInfo memory_info = get_memory_info();
  NetStats network_stats = read_network_stats(network_interface);

  InstantMetrics metrics{uptime_seconds, cpu_data, memory_info, network_stats};

  return metrics;
}

void MetricsCollector::update_metrics() {
  InstantMetrics current_metrics = collect_metrics();

  double cpu_usage = calc_cpu_usage(last_metrics.cpu, current_metrics.cpu);
  double memory_usage = current_metrics.memory_info.usedPercent;

  long tx_delta = current_metrics.network_stats.tx_bytes -
                  last_metrics.network_stats.tx_bytes;
  long rx_delta = current_metrics.network_stats.rx_bytes -
                  last_metrics.network_stats.rx_bytes;
  if (tx_delta < 0) {
    tx_delta = 0;
  }
  if (rx_delta < 0) {
    rx_delta = 0;
  }

  double uptime_delta =
      current_metrics.uptime_seconds - last_metrics.uptime_seconds;

  NetworkUsage network_usage{};
  if (uptime_delta > 0.0) {
    network_usage.tx_rate = tx_delta / uptime_delta;
    network_usage.rx_rate = rx_delta / uptime_delta;
  } else {
    network_usage.tx_rate = 0.0;
    network_usage.rx_rate = 0.0;
  }

  MetricsData new_data{current_metrics.uptime_seconds, cpu_usage, memory_usage,
                       network_usage};

  {
    std::lock_guard<std::mutex> lk(data_lock);
    metrics = new_data;
  }

  last_metrics = current_metrics;
}
