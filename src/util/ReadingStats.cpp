#include "ReadingStats.h"

#include <HalStorage.h>
#include <Logging.h>

ReadingStatsStore::ReadingStatsStore() {
  data.magic = STATS_MAGIC;
  data.version = STATS_VERSION;
  data.totalPagesAllTime = 0;
  data.totalPagesSession = 0;
  data.totalSecondsAllTime = 0;
  data.totalSecondsSession = 0;
  data.reserved[0] = 0;
  data.reserved[1] = 0;
}

void ReadingStatsStore::load() {
  HalFile f;
  if (Storage.openFileForRead("STAT", getFilePath(), f)) {
    Data diskData{};
    const int bytesRead = f.read(reinterpret_cast<uint8_t*>(&diskData), sizeof(diskData));
    if (bytesRead == sizeof(diskData) && diskData.magic == STATS_MAGIC && diskData.version == STATS_VERSION) {
      data = diskData;
      LOG_DBG("STAT", "Loaded stats: all_pages=%u, session_pages=%u, all_sec=%u, session_sec=%u",
              data.totalPagesAllTime, data.totalPagesSession, data.totalSecondsAllTime, data.totalSecondsSession);
      f.close();
      dirty = false;
      return;
    }
    f.close();
  }
  LOG_DBG("STAT", "Initializing fresh reading stats");
  data.magic = STATS_MAGIC;
  data.version = STATS_VERSION;
  data.totalPagesAllTime = 0;
  data.totalPagesSession = 0;
  data.totalSecondsAllTime = 0;
  data.totalSecondsSession = 0;
  data.reserved[0] = 0;
  data.reserved[1] = 0;
  dirty = false;
}

void ReadingStatsStore::save() {
  if (!dirty) return;
  HalFile f;
  if (Storage.openFileForWrite("STAT", getFilePath(), f)) {
    const size_t written = f.write(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
    f.flush();
    if (written == sizeof(data)) {
      dirty = false;
      LOG_DBG("STAT", "Saved stats: all_pages=%u, session_pages=%u, all_sec=%u, session_sec=%u",
              data.totalPagesAllTime, data.totalPagesSession, data.totalSecondsAllTime, data.totalSecondsSession);
    } else {
      LOG_ERR("STAT", "Short write saving stats: %u/%u bytes", (unsigned)written, (unsigned)sizeof(data));
    }
  } else {
    LOG_ERR("STAT", "Failed to open stats file for writing: %s", getFilePath());
  }
}

void ReadingStatsStore::recordPageTurn() {
  data.totalPagesAllTime++;
  data.totalPagesSession++;
  dirty = true;
}

void ReadingStatsStore::addReadingTime(const uint32_t seconds) {
  if (seconds == 0) return;
  data.totalSecondsAllTime += seconds;
  data.totalSecondsSession += seconds;
  dirty = true;
}

void ReadingStatsStore::resetSession() {
  data.totalPagesSession = 0;
  data.totalSecondsSession = 0;
  dirty = true;
}
