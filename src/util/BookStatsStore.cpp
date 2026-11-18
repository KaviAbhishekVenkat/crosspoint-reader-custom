#include "BookStatsStore.h"

#include <Logging.h>

#include <algorithm>

namespace {
int64_t getCurrentEpoch() {
  int64_t now = trustedtime::trustedNow();
  if (now <= 0) {
    const time_t raw = time(nullptr);
    if (raw > 1700000000) {
      now = static_cast<int64_t>(raw);
    }
  }
  return now > 0 ? now : 0;
}
}  // namespace

void BookStatsStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& b : bookStats) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = b.path;
    obj["title"] = b.title;
    obj["author"] = b.author;
    obj["sessions"] = b.sessionsCount;
    obj["pages"] = b.totalPagesRead;
    obj["seconds"] = b.totalReadingSeconds;
    obj["started"] = b.dateStarted;
    obj["lastRead"] = b.dateLastRead;
    obj["finished"] = b.dateFinished;
    obj["progress"] = b.lastProgress;
  }
}

bool BookStatsStore::fromJson(JsonVariantConst doc) {
  bookStats.clear();
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  bookStats.reserve(std::min(arr.size(), MAX_TRACKED_BOOKS));
  for (JsonObjectConst obj : arr) {
    if (bookStats.size() >= MAX_TRACKED_BOOKS) break;
    BookStat b;
    b.path = obj["path"] | "";
    b.title = obj["title"] | "";
    b.author = obj["author"] | "";
    b.sessionsCount = obj["sessions"] | 0;
    b.totalPagesRead = obj["pages"] | 0;
    b.totalReadingSeconds = obj["seconds"] | 0;
    b.dateStarted = obj["started"] | 0LL;
    b.dateLastRead = obj["lastRead"] | 0LL;
    b.dateFinished = obj["finished"] | 0LL;
    b.lastProgress = obj["progress"] | 0;
    bookStats.push_back(b);
  }
  LOG_DBG("BKS", "Loaded %u book stats records", (unsigned)bookStats.size());
  return true;
}

void BookStatsStore::recordSession(const std::string& path, const std::string& title, const std::string& author) {
  if (path.empty()) return;

  const int64_t now = getCurrentEpoch();

  auto it = std::find_if(bookStats.begin(), bookStats.end(), [&](const BookStat& b) { return b.path == path; });
  if (it != bookStats.end()) {
    it->sessionsCount++;
    if (!title.empty()) it->title = title;
    if (!author.empty()) it->author = author;
    if (now > 0) {
      if (it->dateStarted == 0) it->dateStarted = now;
      it->dateLastRead = now;
    }
    if (it != bookStats.begin()) {
      std::rotate(bookStats.begin(), it, it + 1);
    }
  } else {
    BookStat newStat;
    newStat.path = path;
    newStat.title = title;
    newStat.author = author;
    newStat.sessionsCount = 1;
    newStat.totalPagesRead = 0;
    newStat.totalReadingSeconds = 0;
    if (now > 0) {
      newStat.dateStarted = now;
      newStat.dateLastRead = now;
    }
    newStat.dateFinished = 0;
    newStat.lastProgress = 0;
    bookStats.insert(bookStats.begin(), newStat);
    if (bookStats.size() > MAX_TRACKED_BOOKS) {
      bookStats.resize(MAX_TRACKED_BOOKS);
    }
  }
}

void BookStatsStore::recordPageTurn(const std::string& path) {
  if (path.empty()) return;
  auto it = std::find_if(bookStats.begin(), bookStats.end(), [&](const BookStat& b) { return b.path == path; });
  if (it != bookStats.end()) {
    it->totalPagesRead++;
  }
}

void BookStatsStore::addReadingTime(const std::string& path, uint32_t seconds) {
  if (path.empty() || seconds == 0) return;
  auto it = std::find_if(bookStats.begin(), bookStats.end(), [&](const BookStat& b) { return b.path == path; });
  if (it != bookStats.end()) {
    it->totalReadingSeconds += seconds;
    const int64_t now = getCurrentEpoch();
    if (now > 0) {
      it->dateLastRead = now;
      if (it->dateStarted == 0) it->dateStarted = now;
    }
  }
}

void BookStatsStore::updateProgress(const std::string& path, int progressPercent) {
  if (path.empty()) return;
  auto it = std::find_if(bookStats.begin(), bookStats.end(), [&](const BookStat& b) { return b.path == path; });
  if (it != bookStats.end()) {
    it->lastProgress = std::clamp(progressPercent, 0, 100);
    if (it->lastProgress >= 100 && it->dateFinished == 0) {
      const int64_t now = getCurrentEpoch();
      if (now > 0) {
        it->dateFinished = now;
      }
    }
  }
}

const BookStat* BookStatsStore::getBookStat(const std::string& path) const {
  if (path.empty()) return nullptr;
  auto it = std::find_if(bookStats.begin(), bookStats.end(), [&](const BookStat& b) { return b.path == path; });
  if (it != bookStats.end()) {
    return &(*it);
  }
  return nullptr;
}
