#include "main.hpp"
#include "dialog.hpp"
#include "status.hpp"
#include "table.hpp"
#include <ice/context.hpp>
#include <ice/utility.hpp>
#include <comdef.h>
#include <fmt/format.h>
#include <wrl/client.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace std::string_literals;

class Application : public Dialog<Application> {
public:
  // Height           | Note    | Control Options
  // -----------------|---------|------------------------
  // 0                | top     | DIALOGEX 0, 0, 400, 200
  // 2                | padding | TOPMARGIN, 2
  // 2 + 183          | padding | BOTTOMMARGIN, 185
  // 2 + 183 + 2      | task    | 0,187,400,13
  // 2 + 183 + 2 + 13 | bottom  | DIALOGEX 0, 0, 400, 200

  constexpr static auto settings = L"Software\\Xiphos\\Carta";

  Application(HINSTANCE hinstance) noexcept {
    thread_ = std::thread([this]() { io_.run(); });
    Create(hinstance, nullptr, IDD_MAIN, IDI_MAIN);
  }

  ~Application() {
    io_.stop();
    thread_.join();
  }

  auto Io() noexcept {
    return io_.schedule(false);
  }

  void Close() noexcept {
    EnableWindow(hwnd_, FALSE);
    PostMessage(hwnd_, WM_CLOSE, 0, 0);
  }

  void ShowError(std::wstring info, std::wstring_view text = {}) noexcept {
    if (!text.empty()) {
      info += L"\r\n\r\n";
      info += text;
    }
    if (hwnd_ == nullptr || IsWindow(hwnd_)) {
      MessageBox(hwnd_, info.data(), L"Carta Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
  }

  ice::task<void> OnCreate() noexcept {
    // Create status.
    status_ = GetControl(IDC_STATUS);

    // Create table.
    table_ = GetControl(IDC_TABLE);
    for (int col = 0; col < 10; col++) {
      table_.AddColumn(fmt::format(L"Column {}", col).data(), 100);
    }
    table_.Resize(500);

    // Show window.
    WINDOWPLACEMENT wp = {};
    DWORD type = REG_BINARY;
    DWORD size = sizeof(wp);
    if (RegGetValue(HKEY_CURRENT_USER, settings, L"Window", RRF_RT_REG_BINARY, &type, &wp, &size) == ERROR_SUCCESS) {
      wp.showCmd = SW_NORMAL;
      SetWindowPlacement(hwnd_, &wp);
    } else {
      ShowWindow(hwnd_, SW_SHOW);
    }
    co_return;
  }

  ice::task<void> OnClose() noexcept {
    ShowWindow(hwnd_, SW_HIDE);
    WINDOWPLACEMENT wp = {};
    if (GetWindowPlacement(hwnd_, &wp)) {
      RegSetKeyValue(HKEY_CURRENT_USER, settings, L"Window", REG_BINARY, &wp, sizeof(wp));
    }
    DestroyWindow(hwnd_);
    co_return;
  }

  ice::task<void> OnDestroy() noexcept {
    PostQuitMessage(0);
    co_return;
  }

  //ice::task<void> OnTest() noexcept {
  //  EnableWindow(GetControl(IDC_TEST), FALSE);
  //  const auto enable_button = ice::on_scope_exit([this]() { EnableWindow(GetControl(IDC_TEST), TRUE); });
  //
  //  co_await Io();
  //
  //  auto state = status_.Set(L"One...");
  //  Sleep(1000);
  //  state.Set(L"Two...");
  //  Sleep(1000);
  //
  //  // TODO
  //
  //  co_return;
  //}

  BOOL OnMenu(UINT id) noexcept {
    //switch (id) {
    //case IDC_TEST:
    //  OnTest().detach();
    //  return TRUE;
    //}
    return FALSE;
  }

  BOOL OnAccelerator(UINT id) noexcept {
    return FALSE;
  }

  BOOL OnCommand(UINT code, UINT id, HWND hwnd) noexcept {
    switch (code) {
    case 0:
      return OnMenu(id);
    case 1:
      return OnAccelerator(id);
    }
    return FALSE;
  }

  class Cache {
  public:
    Cache(int columns) noexcept : columns_(columns) {
    }

    void Load(int min, int max) noexcept {
      if (min != min_ || max != max_) {
        text_.clear();
        data_.resize(static_cast<std::size_t>((max - min + 1) * columns_) * 2);
        std::size_t pos = 0;
        for (int row = min; row <= max; row++) {
          for (int col = 0; col < columns_; col++) {
            const auto beg = text_.size();
            fmt::format_to(text_, L"{:03}:{:03}", row, col);
            text_.push_back(L'\0');
            const auto end = text_.size();
            data_[pos++] = beg;
            data_[pos++] = end - beg;
          }
        }
      }
      min_ = min;
      max_ = max;
    }

    BOOL Get(LPWSTR dst, int max, int row, int col) noexcept {
      if (row < min_ || row > max_) {
        return FALSE;
      }
      const auto pos = static_cast<std::size_t>((row - min_) * columns_ + col);
      const auto src = text_.data() + data_[pos];
      const auto min = std::min(data_[pos + 1], static_cast<std::size_t>(max));
      std::memcpy(dst, src, min * sizeof(wchar_t));
      return TRUE;
    }

  private:
    int min_ = 0;
    int max_ = 0;
    int columns_ = 0;
    fmt::wmemory_buffer text_;
    std::vector<std::size_t> data_;
  };

  Cache cache_{ 10 };

  BOOL OnGetDispInfo(LVITEM& item) noexcept {
    if (item.mask & LVIF_TEXT) {
      return cache_.Get(item.pszText, item.cchTextMax, item.iItem, item.iSubItem);
    }
    return FALSE;
  }

  BOOL OnCacheHint(NMLVCACHEHINT& hint) noexcept {
    if (hint.iFrom >= 0 && hint.iTo >= hint.iFrom) {
      cache_.Load(hint.iFrom, hint.iTo);
    }
    return TRUE;
  }

  BOOL OnFindItem(NMLVFINDITEM& item) noexcept {
    OutputDebugString(fmt::format(L"OnFindItem: {}\n", item.iStart).data());
    return FALSE;
  }

  BOOL OnNotify(LPNMHDR msg) noexcept {
    if (msg->idFrom == IDC_TABLE) {
      const auto table = GetControl(IDC_TABLE);
      switch (msg->code) {
      case LVN_GETDISPINFO:
        return OnGetDispInfo(reinterpret_cast<NMLVDISPINFO*>(msg)->item);
      case LVN_ODCACHEHINT:
        return OnCacheHint(*reinterpret_cast<NMLVCACHEHINT*>(msg));
      case LVN_ODFINDITEM:
        return OnFindItem(*reinterpret_cast<NMLVFINDITEM*>(msg));
      }
    }
    return FALSE;
  }

  static BOOL Initialize() noexcept {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    return InitCommonControlsEx(&icc);
  }

  static int Run() noexcept {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
  }

private:
  ice::context io_;
  std::thread thread_;
  Status status_;
  Table table_;
};

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int) {
  Application::Initialize();
  Application application(hinstance);
  return Application::Run();
}
