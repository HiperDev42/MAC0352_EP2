#pragma once

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

double get_cpu_usage();

MemoryInfo get_memory_info();
double get_memory_usage();
NetStats get_network_stats();

class MetricsCollector {
  public:
    void start()
    void collect()
    void stop()

  private:
    int thread_id;
    std::mutex data_lock;
    long last_up_time;

    CpuData last_cpu_data;
    double cpu_usage;

    MemoryInfo memory_info;
    NetStats last_network_stats;
    std::string network_interface;
}
