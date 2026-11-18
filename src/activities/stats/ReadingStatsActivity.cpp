#include "ReadingStatsActivity.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "RecentBooksStore.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
const char* const TAB_NAMES[] = {"Overview", "Books", "Bookmarks", "Badges"};

void formatDate(const int64_t epoch, char* buf, const size_t sz) {
  if (epoch <= 0) {
    snprintf(buf, sz, "--");
  } else {
    const time_t t = static_cast<time_t>(epoch);
    struct tm tmInfo;
    localtime_r(&t, &tmInfo);
    strftime(buf, sz, "%d %b %Y", &tmInfo);
  }
}
}  // namespace

ReadingStatsActivity::ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("ReadingStats", renderer, mappedInput) {
  tabPillMaxPad = 10;
}

const char* ReadingStatsActivity::tabLabel(const int index) const {
  if (index >= 0 && index < static_cast<int>(Tab::Count)) {
    return TAB_NAMES[index];
  }
  return "";
}

void ReadingStatsActivity::onEnter() {
  UiTabListActivity::onEnter();
  READING_STATS.load();
  BOOK_STATS.load();
  rebuildRowItems();
}

void ReadingStatsActivity::rebuildRowItems() {
  rowData_.clear();

  switch (tab_) {
    case Tab::Overview: {
      const auto& globalData = READING_STATS.getData();
      const auto& allBooks = BOOK_STATS.getAllStats();
      const uint32_t sessionMins = globalData.totalSecondsSession / 60;
      const uint32_t totalMins = globalData.totalSecondsAllTime / 60;
      const uint32_t totalHours = totalMins / 60;
      const uint32_t totalRemMins = totalMins % 60;

      // Current Session section
      {
        char pgsBuf[32];
        snprintf(pgsBuf, sizeof(pgsBuf), "%u pgs", (unsigned)globalData.totalPagesSession);
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%um", (unsigned)sessionMins);

        rowData_.push_back({"Session Pages", "", pgsBuf, "Current Session", false, false, ""});
        rowData_.push_back({"Session Time", "", timeBuf, "", false, false, ""});
        rowData_.push_back({"Reset Session", "Clears current session counters", "Tap to reset", "", true, true, ""});
      }

      // All-Time Totals section
      {
        char booksBuf[32];
        snprintf(booksBuf, sizeof(booksBuf), "%u books", (unsigned)allBooks.size());
        char allPgsBuf[32];
        snprintf(allPgsBuf, sizeof(allPgsBuf), "%u pgs", (unsigned)globalData.totalPagesAllTime);
        char allTimeBuf[32];
        snprintf(allTimeBuf, sizeof(allTimeBuf), "%uh %um", (unsigned)totalHours, (unsigned)totalRemMins);

        std::string avgPace = "N/A";
        if (globalData.totalPagesAllTime >= 2 && globalData.totalSecondsAllTime >= 30) {
          const uint32_t secPerPage = globalData.totalSecondsAllTime / globalData.totalPagesAllTime;
          const uint32_t pgh = static_cast<uint32_t>((static_cast<uint64_t>(globalData.totalPagesAllTime) * 3600ULL) /
                                                      globalData.totalSecondsAllTime);
          char paceBuf[48];
          snprintf(paceBuf, sizeof(paceBuf), "%um %02us/pg (%u p/h)", (unsigned)(secPerPage / 60),
                   (unsigned)(secPerPage % 60), (unsigned)pgh);
          avgPace = paceBuf;
        }

        rowData_.push_back({"Books in History", "", booksBuf, "All-Time Totals", false, false, ""});
        rowData_.push_back({"Pages Read", "", allPgsBuf, "", false, false, ""});
        rowData_.push_back({"Reading Time", "", allTimeBuf, "", false, false, ""});
        rowData_.push_back({"Average Velocity", "", avgPace, "", false, false, ""});
      }

      // Most Recent / Active Book
      if (!allBooks.empty()) {
        const auto& b = allBooks[0];
        char sessBuf[32];
        snprintf(sessBuf, sizeof(sessBuf), "%u sessions", (unsigned)b.sessionsCount);
        char paceBuf[48];
        snprintf(paceBuf, sizeof(paceBuf), "%s", b.getPaceString().c_str());
        char timeBuf[32];
        const uint32_t bMins = b.totalReadingSeconds / 60;
        snprintf(timeBuf, sizeof(timeBuf), "%uh %um", (unsigned)(bMins / 60), (unsigned)(bMins % 60));

        char statusBuf[32];
        if (b.dateFinished > 0) {
          snprintf(statusBuf, sizeof(statusBuf), "Finished");
        } else {
          snprintf(statusBuf, sizeof(statusBuf), "%d%% In Progress", b.lastProgress);
        }

        char startBuf[20];
        char lastReadBuf[20];
        formatDate(b.dateStarted, startBuf, sizeof(startBuf));
        formatDate(b.dateLastRead, lastReadBuf, sizeof(lastReadBuf));

        const std::string bookSub = b.author.empty() ? "" : ("by " + b.author);
        rowData_.push_back(
            {b.title.empty() ? "Current Book" : b.title, bookSub, statusBuf, "Most Recent Book", true, false, ""});
        rowData_.push_back({"Sessions", "", sessBuf, "", false, false, ""});
        rowData_.push_back({"Reading Pace", "", paceBuf, "", false, false, ""});
        rowData_.push_back({"Time Invested", "", timeBuf, "", false, false, ""});
        rowData_.push_back({"Date Started", "", startBuf, "", false, false, ""});
        rowData_.push_back({"Last Read", "", lastReadBuf, "", false, false, ""});
        rowData_.push_back({"Resume Reading", "Open this book in the reader", "Tap to open", "", true, true, b.path});
      }
      break;
    }

    case Tab::Books: {
      const auto& allBooks = BOOK_STATS.getAllStats();
      if (allBooks.empty()) {
        rowData_.push_back({"No Books Tracked Yet", "Open any EPUB or XTC to record reading stats", "", "Book History",
                            false, false, ""});
      } else {
        for (size_t i = 0; i < allBooks.size(); ++i) {
          const auto& b = allBooks[i];

          // Check bookmark count for this book
          std::vector<BookmarkEntry> bookBms;
          BookmarkFile::load(b.path, bookBms);

          std::string heading = b.title.empty() ? "Untitled Book" : b.title;
          if (!b.author.empty()) {
            heading += " (" + b.author + ")";
          }

          char statusBuf[32];
          if (b.dateFinished > 0) {
            snprintf(statusBuf, sizeof(statusBuf), "Finished (100%%)");
          } else {
            snprintf(statusBuf, sizeof(statusBuf), "%d%% In Progress", b.lastProgress);
          }

          const uint32_t bMins = b.totalReadingSeconds / 60;
          char timeBuf[48];
          snprintf(timeBuf, sizeof(timeBuf), "%uh %um (%u pgs)", (unsigned)(bMins / 60), (unsigned)(bMins % 60),
                   (unsigned)b.totalPagesRead);

          char sessBuf[32];
          snprintf(sessBuf, sizeof(sessBuf), "%u sessions", (unsigned)b.sessionsCount);

          char paceBuf[48];
          snprintf(paceBuf, sizeof(paceBuf), "%s", b.getPaceString().c_str());

          char startBuf[24];
          char lastBuf[24];
          formatDate(b.dateStarted, startBuf, sizeof(startBuf));
          formatDate(b.dateLastRead, lastBuf, sizeof(lastBuf));

          char bmBuf[32];
          snprintf(bmBuf, sizeof(bmBuf), "%u saved", (unsigned)bookBms.size());

          // Clean multi-row layout with zero truncation
          rowData_.push_back({"Reading Progress", "", statusBuf, heading, false, false, ""});
          rowData_.push_back({"Total Reading Time", "", timeBuf, "", false, false, ""});
          rowData_.push_back({"Sessions Logged", "", sessBuf, "", false, false, ""});
          rowData_.push_back({"Reading Pace", "", paceBuf, "", false, false, ""});
          rowData_.push_back({"Date Started", "", startBuf, "", false, false, ""});
          rowData_.push_back({"Date Last Read", "", lastBuf, "", false, false, ""});
          rowData_.push_back({"Bookmarks", "", bmBuf, "", false, false, ""});
          rowData_.push_back({"Resume Reading", "Open this book in the reader", "Tap to open", "", true, true, b.path});
        }
      }
      break;
    }

    case Tab::Bookmarks: {
      const auto& allBooks = BOOK_STATS.getAllStats();

      struct BookmarkItem {
        std::string bookPath;
        std::string bookTitle;
        BookmarkEntry entry;
      };
      std::vector<BookmarkItem> allBookmarks;

      for (const auto& b : allBooks) {
        std::vector<BookmarkEntry> bms;
        if (BookmarkFile::load(b.path, bms) && !bms.empty()) {
          for (const auto& bm : bms) {
            allBookmarks.push_back({b.path, b.title.empty() ? "Untitled Book" : b.title, bm});
          }
        }
      }

      // Also scan recent books if any exist
      for (const auto& rb : RECENT_BOOKS.getBooks()) {
        const auto it = std::find_if(allBookmarks.begin(), allBookmarks.end(),
                                     [&](const BookmarkItem& bi) { return bi.bookPath == rb.path; });
        if (it == allBookmarks.end()) {
          std::vector<BookmarkEntry> bms;
          if (BookmarkFile::load(rb.path, bms) && !bms.empty()) {
            for (const auto& bm : bms) {
              allBookmarks.push_back({rb.path, rb.title.empty() ? "Untitled Book" : rb.title, bm});
            }
          }
        }
      }

      if (allBookmarks.empty()) {
        rowData_.push_back(
            {"No Bookmarks Saved Yet", "Add bookmarks while reading from the reader menu or power/confirm hold", "",
             "Saved Bookmarks", false, false, ""});
      } else {
        std::string currentHeading;
        for (size_t i = 0; i < allBookmarks.size(); ++i) {
          const auto& item = allBookmarks[i];
          std::string heading;
          if (item.bookTitle != currentHeading) {
            currentHeading = item.bookTitle;
            heading = currentHeading;
          }

          const int pct = static_cast<int>(item.entry.percentage * 100.0f + 0.5f);
          char valBuf[16];
          snprintf(valBuf, sizeof(valBuf), "%d%%", pct);

          std::string labelText = item.entry.summary;
          if (labelText.empty()) {
            labelText = item.entry.name.empty() ? "Bookmark" : item.entry.name;
          }

          std::string subText =
              item.entry.name.empty() ? ("Position: " + std::to_string(pct) + "%") : item.entry.name;

          rowData_.push_back({labelText, subText, valBuf, heading, true, true, item.bookPath});
        }
      }
      break;
    }

    case Tab::Badges: {
      const auto& globalData = READING_STATS.getData();
      const auto& allBooks = BOOK_STATS.getAllStats();
      const uint32_t totalBooks = static_cast<uint32_t>(allBooks.size());
      const uint32_t totalPages = globalData.totalPagesAllTime;
      const uint32_t totalSeconds = globalData.totalSecondsAllTime;

      uint32_t finishedBooks = 0;
      uint32_t totalSessions = 0;
      uint32_t booksOver300Pgs = 0;
      for (const auto& b : allBooks) {
        if (b.dateFinished > 0 || b.lastProgress >= 100) finishedBooks++;
        totalSessions += b.sessionsCount;
        if (b.totalPagesRead >= 300) booksOver300Pgs++;
      }

      struct BadgeDef {
        const char* name;
        const char* desc;
        bool unlocked;
        std::string progressStr;
      };

      const BadgeDef badges[] = {
          {"First Blood", "Read your first book", totalBooks >= 1, totalBooks >= 1 ? "Unlocked" : "0/1"},
          {"Warming Up", "Read 3 books", totalBooks >= 3, totalBooks >= 3 ? "Unlocked" : (std::to_string(totalBooks) + "/3")},
          {"Avid Reader", "Read 5 books", totalBooks >= 5, totalBooks >= 5 ? "Unlocked" : (std::to_string(totalBooks) + "/5")},
          {"Bookworm", "Read 10 books", totalBooks >= 10, totalBooks >= 10 ? "Unlocked" : (std::to_string(totalBooks) + "/10")},
          {"Finisher", "Complete at least 1 book 100%", finishedBooks >= 1, finishedBooks >= 1 ? "Unlocked" : "0/1"},
          {"Page Turner", "Turn 500+ pages", totalPages >= 500, totalPages >= 500 ? "Unlocked" : (std::to_string(totalPages) + "/500")},
          {"Century Club", "Turn 1,000+ pages", totalPages >= 1000, totalPages >= 1000 ? "Unlocked" : (std::to_string(totalPages) + "/1000")},
          {"Marathon Reader", "Turn 5,000+ pages", totalPages >= 5000, totalPages >= 5000 ? "Unlocked" : (std::to_string(totalPages) + "/5000")},
          {"The Long Haul", "Read 300+ pages in a single book", booksOver300Pgs >= 1, booksOver300Pgs >= 1 ? "Unlocked" : "Locked"},
          {"Devoted Reader", "Read for 5+ hours", totalSeconds >= 18000, totalSeconds >= 18000 ? "Unlocked" : (std::to_string(totalSeconds / 3600) + "/5h")},
          {"Time Lord", "Read for 20+ hours", totalSeconds >= 72000, totalSeconds >= 72000 ? "Unlocked" : (std::to_string(totalSeconds / 3600) + "/20h")},
          {"Habit Builder", "Log 10+ reading sessions", totalSessions >= 10, totalSessions >= 10 ? "Unlocked" : (std::to_string(totalSessions) + "/10")},
          {"Night Owl", "Log 25+ reading sessions", totalSessions >= 25, totalSessions >= 25 ? "Unlocked" : (std::to_string(totalSessions) + "/25")},
      };

      int unlockedCount = 0;
      for (const auto& bg : badges) {
        if (bg.unlocked) unlockedCount++;
      }

      char summaryHead[48];
      snprintf(summaryHead, sizeof(summaryHead), "Achievements (%d/%d Unlocked)", unlockedCount,
               static_cast<int>(sizeof(badges) / sizeof(badges[0])));

      for (size_t i = 0; i < sizeof(badges) / sizeof(badges[0]); ++i) {
        const auto& bg = badges[i];
        const std::string title = (bg.unlocked ? "[*] " : "[ ] ") + std::string(bg.name);
        rowData_.push_back({title, bg.desc, bg.progressStr, i == 0 ? summaryHead : "", bg.unlocked, false, ""});
      }
      break;
    }

    default:
      break;
  }

  // Populate rowItems_ pointers from rowData_
  rowItems_.clear();
  rowItems_.reserve(rowData_.size());
  for (size_t i = 0; i < rowData_.size(); ++i) {
    const auto& d = rowData_[i];
    fui::ListItem item;
    item.label = d.label.c_str();
    item.subtitle = d.subtitle.empty() ? nullptr : d.subtitle.c_str();
    item.value = d.value.empty() ? nullptr : d.value.c_str();
    item.sectionHeading = d.sectionHeading.empty() ? nullptr : d.sectionHeading.c_str();
    item.bold = d.bold;
    item.enabled = d.enabled;
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }
}

void ReadingStatsActivity::activateIndex(const int index) {
  if (index >= 0 && index < static_cast<int>(rowData_.size())) {
    const auto& row = rowData_[index];
    if (tab_ == Tab::Overview && row.label == "Reset Session") {
      READING_STATS.resetSession();
      READING_STATS.save();
      rebuildRowItems();
      requestUpdate();
      return;
    }
    if (!row.targetBookPath.empty()) {
      if (Storage.exists(row.targetBookPath.c_str())) {
        activityManager.goToReader(row.targetBookPath);
      }
    }
  }
}

void ReadingStatsActivity::onTabAction(const int index) {
  if (tab_ != static_cast<Tab>(index)) {
    tab_ = static_cast<Tab>(index);
    rebuildRowItems();
    auto& n = activeNav();
    n.selected = 0;
    n.followOnBuild = true;
    requestUpdate();
  }
}

void ReadingStatsActivity::stepTab(const int direction) {
  constexpr int count = static_cast<int>(Tab::Count);
  const int next = (static_cast<int>(tab_) + direction + count) % count;
  tab_ = static_cast<Tab>(next);
  rebuildRowItems();
  auto& n = activeNav();
  n.followOnBuild = true;
  requestUpdate();
}

bool ReadingStatsActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    stepTab(-1);
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    stepTab(1);
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (ringPos() == 0) {
      stepTab(1);
    } else {
      const int selected = ringPos() - 1;
      if (selected >= 0 && selected < listCount()) {
        activateIndex(selected);
      }
    }
    return true;
  }
  return false;
}

void ReadingStatsActivity::onBackButton() { onGoHome(HomeMenuItem::READING_STATISTICS); }

void ReadingStatsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});

  buildTabBar(screen);

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 1;
  syncTabListViewport(screen, props);
  screen.list(props);
}
