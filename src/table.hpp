#pragma once
#include <windows.h>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

class Table {
public:
  Table() = default;

  Table(HWND hwnd) noexcept : hwnd_(hwnd) {
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
    ListView_SetExtendedListViewStyleEx(hwnd_, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES);
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
    return ListView_InsertColumn(hwnd_, columns_++, &column) != -1 ? TRUE : FALSE;
  }

  BOOL Resize(std::size_t rows) noexcept {
    return ListView_SetItemCount(hwnd_, static_cast<WPARAM>(rows)) ? TRUE : FALSE;
  }

private:
  HWND hwnd_{ nullptr };
  int columns_ = 0;
};
