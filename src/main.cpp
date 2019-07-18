#include "main.hpp"
#include "dialog.hpp"
#include "status.hpp"
#include "table.hpp"
#include <ice/context.hpp>
#include <ice/utility.hpp>
#include <comdef.h>
#include <fmt/format.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fstream>

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
    status_ = GetControl(IDC_STATUS);
    table_ = GetControl(IDC_TABLE);
    for (int col = 0; col < 10; col++) {
      table_.AddColumn(fmt::format(L"Column {}", col).data(), 100);
    }
    table_.Resize(100'000'000);

    auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
      ShowError(L"Could not initialize COM library.", _com_error(hr).ErrorMessage());
      co_return Close();
    }

    auto factory_ptr = reinterpret_cast<void**>(factory_.GetAddressOf());
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(factory_), factory_ptr);
    if (FAILED(hr)) {
      ShowError(L"Could not create imaging factory.", _com_error(hr).ErrorMessage());
      co_return Close();
    }

    ComPtr<IWICBitmapDecoder> decoder;
    const auto path = L"doc/DIN 5008.jpg";
    hr = factory_->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr)) {
      ShowError(L"Could not create bitmap decoder.", _com_error(hr).ErrorMessage());
      co_return Close();
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
      ShowError(L"Could not get image frame.", _com_error(hr).ErrorMessage());
      co_return Close();
    }

    hr = frame->QueryInterface(__uuidof(IWICBitmapSource), reinterpret_cast<void**>(source_.GetAddressOf()));
    if (FAILED(hr)) {
      ShowError(L"Could not get image source.", _com_error(hr).ErrorMessage());
      co_return Close();
    }

    WINDOWPLACEMENT wp = {};
    DWORD wt = REG_BINARY;
    DWORD ws = sizeof(wp);
    if (RegGetValue(HKEY_CURRENT_USER, settings, L"Window", RRF_RT_REG_BINARY, &wt, &wp, &ws) == ERROR_SUCCESS) {
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
    co_await Io();
    CoUninitialize();
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
    return table_.Set(hint.iFrom, hint.iTo, [this](auto& text, int row, int col) noexcept { fmt::format_to(text, L"{:09}:{:02}", row, col); });
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

  ComPtr<IWICImagingFactory> factory_;
  ComPtr<IWICBitmapSource> source_;
  HBITMAP bitmap_{ nullptr };

  BOOL OnDrawItem(UINT id, LPDRAWITEMSTRUCT draw) noexcept {
    if (id == IDC_PREVIEW) {
      FillRect(draw->hDC, &draw->rcItem, white_);

      ComPtr<IWICBitmapScaler> scaler;
      auto hr = factory_->CreateBitmapScaler(scaler.GetAddressOf());
      if (FAILED(hr)) {
        ShowError(L"Could not create bitmap scaler.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }

      const auto dcx = static_cast<UINT>(draw->rcItem.right - draw->rcItem.left);
      const auto dcy = static_cast<UINT>(draw->rcItem.bottom - draw->rcItem.top);
      hr = scaler->Initialize(source_.Get(), dcx, dcy, WICBitmapInterpolationModeFant);
      if (FAILED(hr)) {
        ShowError(L"Could not initialize scaler.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }

      ComPtr<IWICFormatConverter> converter;
      hr = factory_->CreateFormatConverter(converter.GetAddressOf());
      if (FAILED(hr)) {
        ShowError(L"Could not create format converter.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }

      hr = converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
      if (FAILED(hr)) {
        ShowError(L"Could not initialize converter.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }

      ComPtr<IWICBitmapSource> source;
      hr = converter->QueryInterface(__uuidof(source), reinterpret_cast<void**>(source.GetAddressOf()));
      if (FAILED(hr)) {
        ShowError(L"Could not create scaled source.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }

      WICPixelFormatGUID format = {};
      hr = source->GetPixelFormat(&format);
      if (FAILED(hr)) {
        ShowError(L"Could not get pixel format.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }
      assert(format == GUID_WICPixelFormat32bppBGR);

      UINT scx = 0;
      UINT scy = 0;
      hr = source->GetSize(&scx, &scy);
      if (FAILED(hr)) {
        ShowError(L"Could not get image dimensions.", _com_error(hr).ErrorMessage());
        PostQuitMessage(1);
        return FALSE;
      }
      assert(scx > 0);
      assert(scy > 0);

      BITMAPINFO bmi = {};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = static_cast<LONG>(scx);
      bmi.bmiHeader.biHeight = -static_cast<LONG>(scy);
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      PVOID data = nullptr;
      if (const auto screen = GetDC(nullptr)) {
        if (bitmap_) {
          DeleteObject(bitmap_);
        }
        bitmap_ = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &data, nullptr, 0);
        ReleaseDC(nullptr, screen);
      }

      if (bitmap_ && data) {
        UINT stride = 0;
        hr = UIntMult(scx, sizeof(DWORD), &stride);
        if (FAILED(hr)) {
          ShowError(L"Could not get image stride.", _com_error(hr).ErrorMessage());
          PostQuitMessage(1);
          return FALSE;
        }

        UINT size = 0;
        hr = UIntMult(stride, scy, &size);
        if (FAILED(hr)) {
          ShowError(L"Could not get image size.", _com_error(hr).ErrorMessage());
          PostQuitMessage(1);
          return FALSE;
        }

        hr = source->CopyPixels(nullptr, stride, size, reinterpret_cast<BYTE*>(data));
        if (FAILED(hr)) {
          ShowError(L"Could not copy image data.", _com_error(hr).ErrorMessage());
          PostQuitMessage(1);
          return FALSE;
        }

        if (const auto mem = CreateCompatibleDC(nullptr)) {
          if (const auto old = SelectBitmap(mem, bitmap_)) {
            BITMAP bm;
            if (GetObject(bitmap_, sizeof(bm), &bm) == sizeof(bm)) {
              BitBlt(draw->hDC, 0, 0, bm.bmWidth, bm.bmHeight, mem, 0, 0, SRCCOPY);
              SelectBitmap(mem, old);
            }
          }
          DeleteDC(mem);
        }
      }

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
