#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "specter/ModalWindow.hpp"
#include "specter/PathButtons.hpp"
#include "specter/Table.hpp"
#include "specter/View.hpp"
#include "specter/ViewResources.hpp"

#include <hecl/SystemChar.hpp>

namespace hecl {
class DirectoryEnumerator;
}

namespace specter {
class MessageWindow;
class TextField;

struct IViewManager;

class FileBrowser : public ModalWindow, public IPathButtonsBinding {
public:
  enum class Type { SaveFile, SaveDirectory, OpenFile, OpenDirectory, NewHECLProject, OpenHECLProject };

private:
  Type m_type;
  hecl::SystemString m_path;
  std::vector<hecl::SystemString> m_comps;
  bool m_showingHidden = false;

  class LeftSide : public View {
    friend class FileBrowser;
    FileBrowser& m_fb;
    LeftSide(FileBrowser& fb, ViewResources& res) : View(res, fb), m_fb(fb) {}
    void resized(const boo::SWindowRect& root, const boo::SWindowRect& sub) override;
    void draw(boo::IGraphicsCommandQueue* gfxQ) override;
  } m_left;

  class RightSide : public View {
    friend class FileBrowser;
    FileBrowser& m_fb;
    RightSide(FileBrowser& fb, ViewResources& res) : View(res, fb), m_fb(fb) {}
    void resized(const boo::SWindowRect& root, const boo::SWindowRect& sub) override;
    void draw(boo::IGraphicsCommandQueue* gfxQ) override;
  } m_right;

  ViewChild<std::unique_ptr<SplitView>> m_split;

  void okActivated(bool viaButton);
  struct OKButton : IButtonBinding {
    FileBrowser& m_fb;
    ViewChild<std::unique_ptr<Button>> m_button;
    std::string m_text;
    OKButton(FileBrowser& fb, ViewResources& res, std::string_view text);
    std::string_view name(const Control* control) const override { return m_text; }
    void activated(const Button* button, const boo::SWindowCoord&) override { m_fb.okActivated(true); }
  } m_ok;

  void cancelActivated();
  struct CancelButton : IButtonBinding {
    FileBrowser& m_fb;
    ViewChild<std::unique_ptr<Button>> m_button;
    std::string m_text;
    CancelButton(FileBrowser& fb, ViewResources& res, std::string_view text);
    std::string_view name(const Control* control) const override { return m_text; }
    void activated(const Button* button, const boo::SWindowCoord&) override { m_fb.cancelActivated(); }
  } m_cancel;

  void pathButtonActivated(size_t idx) override;
  ViewChild<std::unique_ptr<PathButtons>> m_pathButtons;

  ViewChild<std::unique_ptr<TextField>> m_fileField;
  struct FileFieldBind : IStringBinding {
    FileBrowser& m_browser;
    std::string m_name;
    FileFieldBind(FileBrowser& browser, const IViewManager& vm);
    std::string_view name(const Control* control) const override { return m_name; }
    void changed(const Control* control, std::string_view val) override {}
  } m_fileFieldBind;

  std::unique_ptr<MessageWindow> m_confirmWindow;

  struct FileListingDataBind : ITableDataBinding, ITableStateBinding {
    FileBrowser& m_fb;

    struct Entry {
      hecl::SystemString m_path;
      std::string m_name;
      std::string m_type;
      std::string m_size;
    };
    std::vector<Entry> m_entries;

    std::string m_nameCol;
    std::string m_typeCol;
    std::string m_sizeCol;

    std::string m_dirStr;
    std::string m_projStr;
    std::string m_fileStr;

    size_t columnCount() const override { return 3; }
    size_t rowCount() const override { return m_entries.size(); }

    std::string_view header(size_t cIdx) const override {
      switch (cIdx) {
      case 0:
        return m_nameCol;
      case 1:
        return m_typeCol;
      case 2:
        return m_sizeCol;
      default:
        break;
      }
      return {};
    }

    std::string_view cell(size_t cIdx, size_t rIdx) const override {
      switch (cIdx) {
      case 0:
        return m_entries.at(rIdx).m_name;
      case 1:
        return m_entries.at(rIdx).m_type;
      case 2:
        return m_entries.at(rIdx).m_size;
      default:
        break;
      }
      return {};
    }

    float m_columnSplits[3] = {0.0f, 0.7f, 0.9f};

    bool columnSplitResizeAllowed() const override { return true; }

    float getColumnSplit(size_t cIdx) const override { return m_columnSplits[cIdx]; }

    void setColumnSplit(size_t cIdx, float split) override { m_columnSplits[cIdx] = split; }

    void updateListing(const hecl::DirectoryEnumerator& dEnum);

    bool m_sizeSort = false;
    SortDirection m_sortDir = SortDirection::Ascending;
    bool m_needsUpdate = false;

    SortDirection getSort(size_t& cIdx) const override {
      cIdx = m_sizeSort ? 2 : 0;
      return m_sortDir;
    }

    void setSort(size_t cIdx, SortDirection dir) override {
      if (cIdx == 1)
        return;
      m_sizeSort = cIdx == 2;
      m_sortDir = dir;
      m_needsUpdate = true;
    }

    void setSelectedRow(size_t rIdx) override;

    void rowActivated(size_t rIdx) override { m_fb.okActivated(false); }

    FileListingDataBind(FileBrowser& fb, const IViewManager& vm);
  } m_fileListingBind;
  ViewChild<std::unique_ptr<Table>> m_fileListing;

  struct BookmarkDataBind : ITableDataBinding, ITableStateBinding {
    FileBrowser& m_fb;
    BookmarkDataBind(FileBrowser& fb) : m_fb(fb) {}

    struct Entry {
      hecl::SystemString m_path;
      std::string m_name;

      Entry(std::pair<hecl::SystemString, std::string>&& path)
      : m_path(std::move(path.first)), m_name(std::move(path.second)) {}

      Entry(hecl::SystemStringView path) : m_path(path) {
        hecl::SystemUTF8Conv utf8(path);
        if (utf8.str().size() == 1 && utf8.str()[0] == '/') {
          m_name = "/";
          return;
        }
        size_t lastSlash = utf8.str().rfind('/');
        if (lastSlash != std::string::npos)
          m_name.assign(utf8.str().cbegin() + lastSlash + 1, utf8.str().cend());
        else
          m_name = utf8.str();
      }
    };
    std::vector<Entry> m_entries;

    size_t columnCount() const override { return 1; }
    size_t rowCount() const override { return m_entries.size(); }

    std::string_view cell(size_t, size_t rIdx) const override { return m_entries.at(rIdx).m_name; }

    void setSelectedRow(size_t rIdx) override {
      if (rIdx != SIZE_MAX)
        m_fb.navigateToPath(m_entries.at(rIdx).m_path);
    }

    void rowActivated(size_t rIdx) override { m_fb.okActivated(true); }
  };

  BookmarkDataBind m_systemBookmarkBind;
  std::unique_ptr<TextView> m_systemBookmarksLabel;
  ViewChild<std::unique_ptr<Table>> m_systemBookmarks;

  BookmarkDataBind m_projectBookmarkBind;
  std::unique_ptr<TextView> m_projectBookmarksLabel;
  ViewChild<std::unique_ptr<Table>> m_projectBookmarks;

  BookmarkDataBind m_recentBookmarkBind;
  std::unique_ptr<TextView> m_recentBookmarksLabel;
  ViewChild<std::unique_ptr<Table>> m_recentBookmarks;

  std::function<void(bool, hecl::SystemStringView)> m_returnFunc;

public:
  FileBrowser(ViewResources& res, View& parentView, std::string_view title, Type type,
              std::function<void(bool, hecl::SystemStringView)> returnFunc);
  FileBrowser(ViewResources& res, View& parentView, std::string_view title, Type type,
              hecl::SystemStringView initialPath, std::function<void(bool, hecl::SystemStringView)> returnFunc);
  ~FileBrowser() override;

  static std::vector<hecl::SystemString> PathComponents(hecl::SystemStringView path);
  static void SyncBookmarkSelections(Table& table, BookmarkDataBind& binding, const hecl::SystemString& sel);

  void navigateToPath(hecl::SystemStringView path);
  bool showingHidden() const { return m_showingHidden; }
  void setShowingHidden(bool showingHidden) {
    m_showingHidden = showingHidden;
    navigateToPath(m_path);
  }
  void updateContentOpacity(float opacity) override;

  void mouseDown(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey) override;
  void mouseUp(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey) override;
  void mouseMove(const boo::SWindowCoord&) override;
  void mouseLeave(const boo::SWindowCoord&) override;
  void scroll(const boo::SWindowCoord&, const boo::SScrollDelta&) override;
  void touchDown(const boo::STouchCoord&, uintptr_t) override;
  void touchUp(const boo::STouchCoord&, uintptr_t) override;
  void touchMove(const boo::STouchCoord&, uintptr_t) override;
  void charKeyDown(unsigned long, boo::EModifierKey, bool) override;
  void specialKeyDown(boo::ESpecialKey, boo::EModifierKey, bool) override;

  void resized(const boo::SWindowRect& root, const boo::SWindowRect& sub) override;
  void think() override;
  void draw(boo::IGraphicsCommandQueue* gfxQ) override;
};

} // namespace specter
