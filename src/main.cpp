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

#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

using Microsoft::WRL::ComPtr;
using namespace std::string_literals;

class Application : public Dialog<Application> {
public:
  // Table
  // ====================================================
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
    table_.Resize(100'000'000);

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

  BOOL OnSize(LONG cx, LONG cy) noexcept {
    const auto thwnd = GetControl(IDC_TABLE);
    const auto phwnd = GetControl(IDC_PREVIEW);
    RECT trc = {};
    GetWindowRect(thwnd, &trc);
    RECT prc = {};
    GetWindowRect(phwnd, &prc);
    MapWindowPoints(nullptr, hwnd_, reinterpret_cast<PPOINT>(&prc), 2);
    const auto dx = (prc.bottom - prc.top) * 210L / 297L - (prc.right - prc.left);
    const auto wp = BeginDeferWindowPos(2);
    constexpr auto tflags = SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE;
    DeferWindowPos(wp, thwnd, nullptr, 0, 0, trc.right - trc.left - dx, trc.bottom - trc.top, tflags);
    constexpr auto pflags = SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    DeferWindowPos(wp, phwnd, nullptr, prc.left - dx, prc.top, prc.right - prc.left + dx, prc.bottom - prc.top, pflags);
    EndDeferWindowPos(wp);
    return TRUE;
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

  BOOL OnGetDispInfo(LVITEM& item) noexcept {
    return table_.Get(item);
  }

  BOOL OnCacheHint(NMLVCACHEHINT& hint) noexcept {
    return table_.Set(hint.iFrom, hint.iTo, [this](auto& text, int row, int col) noexcept {
      fmt::format_to(text, L"{:09}:{:02}", row, col);
    });
  }

  BOOL OnFindItem(NMLVFINDITEM& item) noexcept {
    return FALSE;
  }

  BOOL OnNotify(LPNMHDR msg) noexcept {
    if (msg->idFrom == IDC_TABLE) {
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

  const HBRUSH white_ = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  const HBRUSH gray_ = reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH));

  BOOL OnDrawItem(UINT id, LPDRAWITEMSTRUCT draw) noexcept {
    if (id == IDC_PREVIEW) {
      //FillRect(draw->hDC, &draw->rcItem, white_);
      FrameRect(draw->hDC, &draw->rcItem, gray_);
      return TRUE;
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
