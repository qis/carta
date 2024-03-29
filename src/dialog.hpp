#pragma once
#include <ice/task.hpp>
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <cassert>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

constexpr UINT WM_DIALOG_CREATE = WM_USER + 1;
constexpr UINT WM_DIALOG_RESUME = WM_USER + 2;

template <typename T>
class Dialog {
public:
  struct Layout {
    LONG basex = 1;
    LONG basey = 1;
    LONG minx = 0;
    LONG miny = 0;
    UINT dpi = 96;
  };

  struct Child {
    HWND hwnd = nullptr;
    RECT base = {};
    WORD movex = 0;
    WORD movey = 0;
    WORD sizex = 0;
    WORD sizey = 0;
  };

  class Schedule {
  public:
    constexpr Schedule(HWND hwnd) noexcept : hwnd_(hwnd) {
    }

    bool await_ready() noexcept {
      return GetCurrentThreadId() == GetWindowThreadProcessId(hwnd_, nullptr);
    }

    void await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept {
      PostMessage(hwnd_, WM_DIALOG_CREATE, 0, reinterpret_cast<LPARAM>(coroutine.address()));
    }

    constexpr void await_resume() noexcept {
    }

  private:
    const HWND hwnd_;
  };

  Dialog() noexcept = default;

  Dialog(Dialog&& other) = delete;
  Dialog(const Dialog& other) = delete;
  Dialog& operator=(Dialog&& other) = delete;
  Dialog& operator=(const Dialog& other) = delete;

  virtual ~Dialog() = default;

  HWND Create(HINSTANCE hinstance, HWND parent, UINT id, UINT icon = 0) noexcept {
    hinstance_ = hinstance;
    id_ = id;
    icon_ = icon;
    return CreateDialogParam(hinstance, MAKEINTRESOURCE(id), parent, Proc, reinterpret_cast<LPARAM>(this));
  }

  HWND GetControl(int id) noexcept {
    return GetDlgItem(hwnd_, id);
  }

  Schedule Ui() const noexcept {
    return hwnd_;
  }

  ice::task<void> OnCreate() noexcept = delete;
  ice::task<void> OnClose() noexcept = delete;
  ice::task<void> OnDestroy() noexcept = delete;
  BOOL OnSize(LONG cx, LONG cy) noexcept = delete;
  BOOL OnDpiChanged(UINT dpi, LPCRECT rc) noexcept = delete;
  BOOL OnGetMinMaxInfo(LPMINMAXINFO mm) noexcept = delete;
  BOOL OnCommand(UINT code, UINT id, HWND hwnd) noexcept = delete;
  BOOL OnNotify(LPNMHDR msg) noexcept = delete;
  BOOL OnDrawItem(UINT id, LPDRAWITEMSTRUCT draw) noexcept = delete;
  BOOL OnMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept = delete;

  static void SetIcon(HINSTANCE hinstance, HWND hwnd, UINT dpi, UINT id, WPARAM type) noexcept {
    const auto size = GetIconSize(dpi, type);
    const auto icon = LoadImage(hinstance, MAKEINTRESOURCE(id), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
    SendMessage(hwnd, WM_SETICON, type, reinterpret_cast<LPARAM>(icon));
  }

  static int GetIconSize(UINT dpi, WPARAM type) noexcept {
    switch (dpi) {
    case 96:
      return type == ICON_SMALL ? 16 : 32;
    case 120:
      return type == ICON_SMALL ? 20 : 40;
    case 144:
      return type == ICON_SMALL ? 24 : 48;
    case 192:
      return type == ICON_SMALL ? 32 : 64;
    }
    return type == ICON_SMALL ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON);
  }

protected:
  HINSTANCE hinstance_{ nullptr };
  UINT id_{ 0 };
  UINT icon_{ 0 };
  HWND hwnd_{ nullptr };
  Layout layout_;
  std::vector<Child> children_;

private:
  __forceinline BOOL OnDialogCreate() noexcept {
    RECT rc = {};
    GetClientRect(hwnd_, &rc);
    const auto cx = rc.right - rc.left;
    const auto cy = rc.bottom - rc.top;
    GetWindowRect(hwnd_, &rc);
    const auto minx = rc.right - rc.left;
    const auto miny = rc.bottom - rc.top;
    const auto dpi = GetDpiForWindow(hwnd_);
    layout_ = { cx, cy, minx, miny, dpi };
    if (const auto hres = FindResource(hinstance_, MAKEINTRESOURCE(id_), TEXT("AFX_DIALOG_LAYOUT"))) {
      if (const auto hmem = LoadResource(hinstance_, hres)) {
        const auto size = SizeofResource(nullptr, hres) / sizeof(WORD);
        auto data = static_cast<const WORD*>(LockResource(hmem));
        assert(size % 4 == 1 && *data == 0);
        data++;
        children_.clear();
        children_.reserve(size / 4);
        auto hwnd = GetWindow(hwnd_, GW_CHILD);
        for (DWORD i = 0; i < size / 4; i++) {
          Child child;
          child.hwnd = hwnd;
          GetWindowRect(hwnd, &child.base);
          MapWindowPoints(nullptr, hwnd_, reinterpret_cast<PPOINT>(&child.base), 2);
          child.movex = *data++;
          child.movey = *data++;
          child.sizex = *data++;
          child.sizey = *data++;
          children_.push_back(child);
          SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);
          hwnd = GetWindow(hwnd, GW_HWNDNEXT);
        }
      }
    }
    OnDialogSize(cx, cy);
    SetIcon(hinstance_, hwnd_, dpi, icon_, ICON_SMALL);
    SetIcon(hinstance_, hwnd_, dpi, icon_, ICON_BIG);
    if constexpr (&T::OnCreate != &Dialog::OnCreate) {
      static_cast<T*>(this)->OnCreate().detach();
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogClose() noexcept {
    if constexpr (&T::OnClose != &Dialog::OnClose) {
      static_cast<T*>(this)->OnClose().detach();
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogDestroy() noexcept {
    if constexpr (&T::OnDestroy != &Dialog::OnDestroy) {
      static_cast<T*>(this)->OnDestroy().detach();
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogSize(LONG cx, LONG cy) noexcept {
    const auto dpi = GetDpiForWindow(hwnd_);
    const auto scale = dpi / static_cast<double>(layout_.dpi);
    const auto dx = cx - (dpi == layout_.dpi ? layout_.basex : static_cast<LONG>(layout_.basex * scale));
    const auto dy = cy - (dpi == layout_.dpi ? layout_.basey : static_cast<LONG>(layout_.basey * scale));
    const auto wp = BeginDeferWindowPos(static_cast<int>(children_.size()));
    for (const auto& child : children_) {
      auto rc = child.base;
      if (dpi != layout_.dpi) {
        rc.left = static_cast<LONG>(rc.left * scale);
        rc.top = static_cast<LONG>(rc.top * scale);
        rc.right = static_cast<LONG>(rc.right * scale);
        rc.bottom = static_cast<LONG>(rc.bottom * scale);
      }
      if (dx >= 0 && child.movex) {
        rc.left += static_cast<LONG>(dx * (child.movex / 100.0));
        rc.right += static_cast<LONG>(dx * (child.movex / 100.0));
      }
      if (dy >= 0 && child.movey) {
        rc.top += static_cast<LONG>(dy * (child.movey / 100.0));
        rc.bottom += static_cast<LONG>(dy * (child.movey / 100.0));
      }
      if (dx >= 0 && child.sizex) {
        rc.right += static_cast<LONG>(dx * (child.sizex / 100.0));
      }
      if (dy >= 0 && child.sizey) {
        rc.bottom += static_cast<LONG>(dy * (child.sizey / 100.0));
      }
      constexpr UINT flags = SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS;
      DeferWindowPos(wp, child.hwnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, flags);
    }
    EndDeferWindowPos(wp);
    if constexpr (&T::OnSize != &Dialog::OnSize) {
      return static_cast<T*>(this)->OnSize(cx, cy);
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogDpiChanged(UINT dpi, LPCRECT rc) noexcept {
    SetIcon(hinstance_, hwnd_, dpi, IDI_MAIN, ICON_SMALL);
    SetIcon(hinstance_, hwnd_, dpi, IDI_MAIN, ICON_BIG);
    if constexpr (&T::OnDpiChanged != &Dialog::OnDpiChanged) {
      return static_cast<T*>(this)->OnDpiChanged(dpi, rc);
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogGetMinMaxInfo(LPMINMAXINFO mm) noexcept {
    const auto dpi = GetDpiForWindow(hwnd_);
    const auto scale = dpi / static_cast<double>(layout_.dpi);
    if (layout_.minx) {
      mm->ptMinTrackSize.x = static_cast<LONG>(layout_.minx * scale);
    }
    if (layout_.miny) {
      mm->ptMinTrackSize.y = static_cast<LONG>(layout_.miny * scale);
    }
    if constexpr (&T::OnGetMinMaxInfo != &Dialog::OnGetMinMaxInfo) {
      return static_cast<T*>(this)->OnGetMinMaxInfo(mm);
    }
    return TRUE;
  }

  __forceinline BOOL OnDialogCommand(UINT code, UINT id, HWND hwnd) noexcept {
    if constexpr (&T::OnCommand != &Dialog::OnCommand) {
      return static_cast<T*>(this)->OnCommand(code, id, hwnd);
    }
    return FALSE;
  }

  __forceinline BOOL OnDialogNotify(LPNMHDR msg) noexcept {
    if constexpr (&T::OnNotify != &Dialog::OnNotify) {
      return static_cast<T*>(this)->OnNotify(msg);
    }
    return FALSE;
  }

  __forceinline BOOL OnDialogDrawItem(UINT id, LPDRAWITEMSTRUCT draw) noexcept {
    if constexpr (&T::OnDrawItem != &Dialog::OnDrawItem) {
      return static_cast<T*>(this)->OnDrawItem(id, draw);
    }
    return FALSE;
  }

  static INT_PTR CALLBACK Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_INITDIALOG) {
      SetWindowLongPtr(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(lparam));
      PostMessage(hwnd, WM_DIALOG_CREATE, 0, 0);
    } else if (const auto dialog = reinterpret_cast<Dialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
      switch (message) {
      case WM_DIALOG_CREATE:
        dialog->hwnd_ = hwnd;
        return dialog->OnDialogCreate();
      case WM_CLOSE:
        return dialog->OnDialogClose();
      case WM_DESTROY:
        return dialog->OnDialogDestroy();
      case WM_SIZE:
        return dialog->OnDialogSize(LOWORD(lparam), HIWORD(lparam));
      case WM_DPICHANGED:
        return dialog->OnDialogDpiChanged(HIWORD(wparam), reinterpret_cast<LPCRECT>(lparam));
      case WM_GETMINMAXINFO:
        return dialog->OnDialogGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lparam));
      case WM_COMMAND:
        return dialog->OnDialogCommand(HIWORD(wparam), LOWORD(wparam), reinterpret_cast<HWND>(lparam));
      case WM_NOTIFY:
        return dialog->OnDialogNotify(reinterpret_cast<LPNMHDR>(lparam));
      case WM_DRAWITEM:
        return dialog->OnDialogDrawItem(static_cast<UINT>(wparam), reinterpret_cast<LPDRAWITEMSTRUCT>(lparam));
      case WM_CTLCOLORDLG:
        return reinterpret_cast<UINT_PTR>(GetStockObject(COLOR_WINDOWFRAME));
      case WM_DIALOG_RESUME:
        if (lparam) {
          std::experimental::coroutine_handle<>::from_address(reinterpret_cast<void*>(lparam)).resume();
        }
        return TRUE;
      }
      if constexpr (&T::OnMessage != &Dialog::OnMessage) {
        if (static_cast<T*>(dialog)->OnMessage(hwnd, message, wparam, lparam)) {
          return TRUE;
        }
      }
    }
    return FALSE;
  }
};
