#pragma once

#include <cstdint>
#include <cstddef>

class ReadingStatsStore {
 public:
  struct Data {
    uint32_t magic;
    uint32_t version;
    uint32_t totalPagesAllTime;
    uint32_t totalPagesSession;
    uint32_t totalSecondsAllTime;
    uint32_t totalSecondsSession;
    uint32_t reserved[2];
  };

  static constexpr uint32_t STATS_MAGIC = 0x53544154;  // 'STAT'
  static constexpr uint32_t STATS_VERSION = 1;
  static const char* getFilePath() { return "/.crosspoint/reading_stats.bin"; }

  static ReadingStatsStore& getInstance() {
    static ReadingStatsStore instance;
    return instance;
  }

  void load();
  void save();

  void recordPageTurn();
  void addReadingTime(uint32_t seconds);
  void resetSession();

  const Data& getData() const { return data; }

 private:
  ReadingStatsStore();
  Data data{};
  bool dirty = false;
};

#define READING_STATS ReadingStatsStore::getInstance()
