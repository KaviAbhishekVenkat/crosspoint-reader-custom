#pragma once

#include <string>
#include <vector>

#include "BookmarkEntry.h"
#include "activities/UiTabListActivity.h"
#include "util/BookmarkFile.h"
#include "util/BookStatsStore.h"
#include "util/ReadingStats.h"

class ReadingStatsActivity final : public UiTabListActivity {
 public:
  enum class Tab : uint8_t {
    Overview = 0,
    Books = 1,
    Bookmarks = 2,
    Badges = 3,
    Count = 4
  };

  ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 protected:
  int listCount() const override { return static_cast<int>(rowItems_.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Reading Stats"; }

  int tabCount() const override { return static_cast<int>(Tab::Count); }
  int activeTab() const override { return static_cast<int>(tab_); }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;
  bool handleButtons() override;
  void onBackButton() override;

 private:
  Tab tab_ = Tab::Overview;
  void rebuildRowItems();

  struct RowData {
    std::string label;
    std::string subtitle;
    std::string value;
    std::string sectionHeading;
    bool bold = false;
    bool enabled = true;
    std::string targetBookPath;
  };

  std::vector<RowData> rowData_;
  std::vector<freeink::ui::ListItem> rowItems_;
};
