#pragma once
#include <windows.h>
#include <fmt/format.h>
#include <algorithm>
#include <cassert>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

//
// BOOL OnGetDispInfo(LVITEM& item) noexcept {
//   return table_.Get(item);
// }
//
// BOOL OnCacheHint(NMLVCACHEHINT& hint) noexcept {
//   return table_.Set(hint.iFrom, hint.iTo, [this](auto& text, int row, int col) noexcept {
//     fmt::format_to(text, L"{:09}:{:02}", row, col);
//   });
// }
//
// BOOL OnFindItem(NMLVFINDITEM& item) noexcept {
//   return FALSE;
// }
//
// BOOL OnNotify(LPNMHDR msg) noexcept {
//   if (msg->idFrom == IDC_TABLE) {
//     switch (msg->code) {
//     case LVN_GETDISPINFO:
//       return OnGetDispInfo(reinterpret_cast<NMLVDISPINFO*>(msg)->item);
//     case LVN_ODCACHEHINT:
//       return OnCacheHint(*reinterpret_cast<NMLVCACHEHINT*>(msg));
//     case LVN_ODFINDITEM:
//       return OnFindItem(*reinterpret_cast<NMLVFINDITEM*>(msg));
//     }
//   }
//   return FALSE;
// }
//

class Table {
public:
  Table() = default;

  Table(HWND hwnd) noexcept : hwnd_(hwnd) {
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES);
    ListView_DeleteAllItems(hwnd_);
  }

  void Reset() noexcept {
    max_ = 0;
    min_ = 0;
    cols_ = 0;
    data_ = {};
    text_ = fmt::wmemory_buffer{};
    ListView_DeleteAllItems(hwnd_);
  }

  BOOL AddColumn(LPCWSTR text, int width = 0, int format = LVCFMT_LEFT) noexcept {
    LV_COLUMN column = {};
    column.mask = LVCF_FMT | LVCF_TEXT;
    if (width > 0) {
      column.mask |= LVCF_WIDTH;
      column.cx = 100;
    }
    column.fmt = format;
    column.pszText = const_cast<LPWSTR>(text);
    return ListView_InsertColumn(hwnd_, cols_++, &column) != -1 ? TRUE : FALSE;
  }

  BOOL Resize(std::size_t rows) noexcept {
    assert(rows <= 100'000'000);
    return ListView_SetItemCountEx(hwnd_, static_cast<WPARAM>(rows), LVSICF_NOSCROLL) ? TRUE : FALSE;
  }

  template <typename Callback>
  BOOL Set(int min, int max, Callback callback) noexcept {
    if (min < 0 || max < min) {
      return FALSE;
    }
    text_.clear();
    data_.resize(static_cast<std::size_t>((max - min + 1) * cols_) + 1);
    std::size_t pos = 0;
    for (int row = min; row <= max; row++) {
      for (int col = 0; col < cols_; col++) {
        data_[pos++] = text_.size();
        callback(text_, row, col);
        text_.push_back(L'\0');
      }
    }
    data_[pos++] = text_.size();
    min_ = min;
    max_ = max;
    return TRUE;
  }

  BOOL Get(LVITEM& item) noexcept {
    assert(item.iSubItem >= 0);
    assert(item.iSubItem < cols_);
    if (!(item.mask & LVIF_TEXT) || item.iItem < min_ || item.iItem > max_) {
      return FALSE;
    }
    const auto pos = static_cast<std::size_t>((item.iItem - min_) * cols_ + item.iSubItem);
    const auto beg = data_[pos];
    const auto end = data_[pos + 1];
    const auto max = std::min(static_cast<int>(end - beg), item.cchTextMax) - 1;
    std::memcpy(item.pszText, text_.data() + beg, static_cast<std::size_t>(max) * sizeof(wchar_t));
    item.pszText[max] = L'\0';
    return TRUE;
  }

private:
  int min_ = 0;
  int max_ = 0;
  int cols_ = 0;
  fmt::wmemory_buffer text_;
  std::vector<std::size_t> data_;
  HWND hwnd_{ nullptr };
};
