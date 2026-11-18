#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct BookStat {
  std::string path;
  std::string title;
  std::string author;
  uint32_t sessionsCount = 0;
  uint32_t totalPagesRead = 0;
  uint32_t totalReadingSeconds = 0;
  int64_t dateStarted = 0;   // Unix epoch
  int64_t dateLastRead = 0;  // Unix epoch
  int64_t dateFinished = 0;  // Unix epoch (0 if not finished)
  int lastProgress = 0;      // 0..100%

  std::string getPaceString() const {
    if (totalPagesRead < 2 || totalReadingSeconds < 30) {
      return "N/A";
    }
    const uint32_t secPerPage = totalReadingSeconds / totalPagesRead;
    const uint32_t min = secPerPage / 60;
    const uint32_t sec = secPerPage % 60;
    const uint32_t pagesPerHour = static_cast<uint32_t>((static_cast<uint64_t>(totalPagesRead) * 3600ULL) / totalReadingSeconds);

    char buf[48];
    if (min > 0) {
      snprintf(buf, sizeof(buf), "%um %02us/pg (%u p/h)", (unsigned)min, (unsigned)sec, (unsigned)pagesPerHour);
    } else {
      snprintf(buf, sizeof(buf), "%us/pg (%u p/h)", (unsigned)sec, (unsigned)pagesPerHour);
    }
    return std::string(buf);
  }
};

class BookStatsStore : public PersistableStore<BookStatsStore> {
 public:
  static constexpr size_t MAX_TRACKED_BOOKS = 100;

 private:
  std::vector<BookStat> bookStats;

  BookStatsStore() = default;
  ~BookStatsStore() = default;

  friend class PersistableStore<BookStatsStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/book_stats.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool load() { return loadFromFile(); }
  bool save() { return saveToFile(); }

  void recordSession(const std::string& path, const std::string& title, const std::string& author);
  void recordPageTurn(const std::string& path);
  void addReadingTime(const std::string& path, uint32_t seconds);
  void updateProgress(const std::string& path, int progressPercent);

  const BookStat* getBookStat(const std::string& path) const;
  const std::vector<BookStat>& getAllStats() const { return bookStats; }
  size_t getBookCount() const { return bookStats.size(); }
};

#define BOOK_STATS BookStatsStore::getInstance()
