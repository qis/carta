#include "main.hpp"
#include "dialog.hpp"
#include "status.hpp"
#include <ice/context.hpp>
#include <ice/utility.hpp>
#include <comdef.h>
#include <wrl/client.h>
#include <thread>

using Microsoft::WRL::ComPtr;
using namespace std::string_literals;

class Application : public Dialog<Application> {
public:
  constexpr static auto settings = L"Software\\Xiphos\\Carta";

  Application(HINSTANCE hinstance) noexcept {
    thread_ = std::thread([this]() { io_.run(); });
    Create(hinstance, nullptr, IDD_MAIN, IDI_MAIN);
  }

  ~Application() {
    io_.stop();
    thread_.join();
  }

  ice::schedule Io() noexcept {
    return io_;
  }

  void Exit() noexcept {
    EnableWindow(hwnd_, FALSE);
    PostMessage(hwnd_, WM_CLOSE, 0, 0);
  }

  void ShowError(std::wstring info, std::wstring_view text = {}) noexcept {
    if (!text.empty()) {
      info += L"\r\n\r\n";
      info += text;
    }
    MessageBox(hwnd_, info.data(), L"Carta Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }

  ice::task<void> OnCreate() noexcept {
    status_.Create(GetControl(IDC_STATUS));
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

  ice::task<void> OnTest() noexcept {
    EnableWindow(GetControl(IDC_TEST), FALSE);
    const auto enable_button = ice::on_scope_exit([this]() { EnableWindow(GetControl(IDC_TEST), TRUE); });

    co_await Io();

    auto state = status_.Set(L"Initializing COM library...");
    Sleep(1000);
    state.Set(L"Initializing RAPI library...");
    Sleep(1000);

    // TODO

    co_return;
  }

  BOOL OnMenu(UINT id) noexcept {
    switch (id) {
    case IDC_TEST:
      OnTest().detach();
      return TRUE;
    }
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

  static BOOL Initialize() noexcept {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
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
};

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int) {
  Application::Initialize();
  Application application(hinstance);
  return Application::Run();
}
