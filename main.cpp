#include <windows.h>
#include <windowsx.h>
#include <map>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <string>
#include <strsafe.h>
#include <vector>
#include <uxtheme.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")

#define WM_TRAYICON (WM_USER + 1)
#define IDM_NEW_BOX 1001
#define IDM_EXIT 1002

const WCHAR szMainClassName[] = L"DeskManageHiddenApp";
const WCHAR szBoxClassName[] = L"DeskManageBoxClass";

HINSTANCE g_hInst;
HWND g_hMainWnd;
UINT g_uTrayID = 1;
HHOOK g_hMsgHook = NULL;
int g_BoxCounter = 1;

struct BoxContext {
  HWND hwnd;
  IExplorerBrowser *pBrowser;
  std::wstring folderPath;
  int alpha;
};
std::map<HWND, BoxContext *> g_Boxes;

std::wstring GetBaseFolder() {
  WCHAR path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path))) {
    PathAppendW(path, L".DeskManageData");
    CreateDirectoryW(path, NULL);
    SetFileAttributesW(path, FILE_ATTRIBUTE_HIDDEN);
    return std::wstring(path);
  }
  return L"";
}

void MoveFilesToDesktopAndClean(HWND hwndBox, const std::wstring &folderPath) {
  WCHAR szDesktop[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, szDesktop))) {
    WCHAR szFrom[MAX_PATH + 2] = {0};
    StringCchPrintfW(szFrom, MAX_PATH, L"%s\\*", folderPath.c_str());
    WCHAR szTo[MAX_PATH + 2] = {0};
    StringCchCopyW(szTo, MAX_PATH, szDesktop);

    SHFILEOPSTRUCTW sfo = {0};
    sfo.hwnd = hwndBox;
    sfo.wFunc = FO_MOVE;
    sfo.pFrom = szFrom;
    sfo.pTo = szTo;
    sfo.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT | FOF_RENAMEONCOLLISION;

    SHFileOperationW(&sfo);
  }
  RemoveDirectoryW(folderPath.c_str());
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
  if (code >= 0 && wParam == PM_REMOVE) {
    MSG *pMsg = (MSG *)lParam;
    if (pMsg->message == WM_MOUSEWHEEL) {
      HWND hRoot = GetAncestor(pMsg->hwnd, GA_ROOT);
      if (hRoot && g_Boxes.find(hRoot) != g_Boxes.end()) {
        BoxContext *ctx = g_Boxes[hRoot];
        int delta = (short)HIWORD(pMsg->wParam);
        ctx->alpha += (delta > 0) ? 15 : -15;
        if (ctx->alpha < 40) ctx->alpha = 40;
        if (ctx->alpha > 255) ctx->alpha = 255;
        SetLayeredWindowAttributes(hRoot, 0, ctx->alpha, LWA_ALPHA);
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
          pMsg->message = WM_NULL;
        }
      }
    }
  }
  return CallNextHookEx(g_hMsgHook, code, wParam, lParam);
}

#define BORDER_WIDTH 8
#define CAPTION_HEIGHT 30

LRESULT CALLBACK BoxWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    if (g_Boxes.find(hwnd) != g_Boxes.end()) {
      BoxContext *ctx = g_Boxes[hwnd];
      for (UINT i = 0; i < count; i++) {
        WCHAR filePath[MAX_PATH];
        DragQueryFileW(hDrop, i, filePath, MAX_PATH);
        
        IShellLinkW *psl;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl)))) {
          psl->SetPath(filePath);
          IPersistFile *ppf;
          if (SUCCEEDED(psl->QueryInterface(IID_PPV_ARGS(&ppf)))) {
            WCHAR linkPath[MAX_PATH];
            StringCchPrintfW(linkPath, MAX_PATH, L"%s\\%s.lnk", ctx->folderPath.c_str(), PathFindFileNameW(filePath));
            ppf->Save(linkPath, TRUE);
            ppf->Release();
          }
          psl->Release();
        }
      }
      if (ctx->pBrowser) {
        IFolderView *pView;
        if (SUCCEEDED(ctx->pBrowser->GetCurrentView(IID_PPV_ARGS(&pView)))) {
          pView->Refresh();
          pView->Release();
        }
      }
    }
    DragFinish(hDrop);
    return 0;
  }
  case WM_NCHITTEST: {
    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    RECT rc;
    GetWindowRect(hwnd, &rc);

    bool isLeft = pt.x < rc.left + BORDER_WIDTH;
    bool isRight = pt.x > rc.right - BORDER_WIDTH;
    bool isTop = pt.y < rc.top + BORDER_WIDTH;
    bool isBottom = pt.y > rc.bottom - BORDER_WIDTH;

    if (isTop && isLeft) return HTTOPLEFT;
    if (isTop && isRight) return HTTOPRIGHT;
    if (isBottom && isLeft) return HTBOTTOMLEFT;
    if (isBottom && isRight) return HTBOTTOMRIGHT;
    if (isLeft) return HTLEFT;
    if (isRight) return HTRIGHT;
    if (isTop) return HTTOP;
    if (isBottom) return HTBOTTOM;

    if (pt.y < rc.top + CAPTION_HEIGHT) {
      RECT rcClose = {rc.right - rc.left - 40, 0, rc.right - rc.left, CAPTION_HEIGHT};
      POINT ptClient = pt;
      ScreenToClient(hwnd, &ptClient);
      if (PtInRect(&rcClose, ptClient)) {
        return HTCLIENT;
      }
      return HTCAPTION;
    }
    return HTCLIENT;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    RECT rcCaption = rc;
    rcCaption.bottom = CAPTION_HEIGHT;
    HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rcCaption, hBrush);
    DeleteObject(hBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 200, 200));
    std::wstring title = L"DesktopBox";
    TextOutW(hdc, 10, 8, title.c_str(), (int)title.length());

    RECT rcClose = {rc.right - 40, 0, rc.right, CAPTION_HEIGHT};
    SetTextColor(hdc, RGB(255, 80, 80));
    DrawTextW(hdc, L"X", -1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT rcBody = rc;
    rcBody.top = CAPTION_HEIGHT;
    HBRUSH hBodyBrush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &rcBody, hBodyBrush);
    DeleteObject(hBodyBrush);

    HBRUSH hBorder = CreateSolidBrush(RGB(60, 60, 60));
    RECT rcLeft = {0, 0, 1, rc.bottom};
    FillRect(hdc, &rcLeft, hBorder);
    RECT rcRight = {rc.right - 1, 0, rc.right, rc.bottom};
    FillRect(hdc, &rcRight, hBorder);
    RECT rcTop = {0, 0, rc.right, 1};
    FillRect(hdc, &rcTop, hBorder);
    RECT rcBottom = {0, rc.bottom - 1, rc.right, rc.bottom};
    FillRect(hdc, &rcBottom, hBorder);
    DeleteObject(hBorder);

    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_LBUTTONDOWN: {
    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    RECT rc;
    GetClientRect(hwnd, &rc);
    RECT rcClose = {rc.right - 40, 0, rc.right, CAPTION_HEIGHT};
    if (PtInRect(&rcClose, pt)) {
      if (MessageBoxW(hwnd, L"Close this folder?", L"DeskManage", MB_YESNO | MB_ICONWARNING) == IDYES) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
      }
    }
    return 0;
  }
  case WM_SIZE: {
    if (g_Boxes.find(hwnd) != g_Boxes.end()) {
      BoxContext *ctx = g_Boxes[hwnd];
      if (ctx->pBrowser) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.top += CAPTION_HEIGHT;
        rc.left += 1;
        rc.right -= 1;
        rc.bottom -= 1;
        ctx->pBrowser->SetRect(NULL, rc);
      }
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
  }
  case WM_CLOSE: {
    if (g_Boxes.find(hwnd) != g_Boxes.end()) {
      BoxContext *ctx = g_Boxes[hwnd];
      if (ctx->pBrowser) {
        ctx->pBrowser->Destroy();
        ctx->pBrowser->Release();
        ctx->pBrowser = NULL;
      }
      MoveFilesToDesktopAndClean(hwnd, ctx->folderPath);
      g_Boxes.erase(hwnd);
      delete ctx;
    }
    DestroyWindow(hwnd);
    return 0;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateNewBox() {
  std::wstring baseFolder = GetBaseFolder();
  WCHAR folderName[MAX_PATH];
  StringCchPrintfW(folderName, MAX_PATH, L"%s\\Box_%d", baseFolder.c_str(), g_BoxCounter++);
  CreateDirectoryW(folderName, NULL);

  HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES, szBoxClassName, L"Box", WS_POPUP | WS_CLIPCHILDREN, 200, 200, 400, 300, NULL, NULL, g_hInst, NULL);
  SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);
  DragAcceptFiles(hwnd, TRUE);

  BoxContext *ctx = new BoxContext();
  ctx->hwnd = hwnd;
  ctx->folderPath = folderName;
  ctx->alpha = 200;
  ctx->pBrowser = NULL;

  g_Boxes[hwnd] = ctx;

  if (SUCCEEDED(CoCreateInstance(CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ctx->pBrowser)))) {
    RECT rc = {0, CAPTION_HEIGHT, 400, 300};
    FOLDERSETTINGS fs = {0};
    fs.ViewMode = FVM_ICON;
    fs.fFlags = FWF_AUTOARRANGE | FWF_NOWEBVIEW | FWF_TRANSPARENT | FWF_HIDEFILENAMES | FWF_NODROPTARGET;

    ctx->pBrowser->Initialize(hwnd, &rc, &fs);
    ctx->pBrowser->SetOptions(EBO_NOBORDER);
    PIDLIST_ABSOLUTE pidl = NULL;
    if (SUCCEEDED(SHParseDisplayName(folderName, NULL, &pidl, 0, NULL))) {
      ctx->pBrowser->BrowseToIDList(pidl, SBSP_ABSOLUTE);
      
      IFolderView *pView;
      if (SUCCEEDED(ctx->pBrowser->GetCurrentView(IID_PPV_ARGS(&pView)))) {
        HWND hwndView;
        if (SUCCEEDED(pView->GetWindow(&hwndView))) {
          SetWindowTheme(hwndView, L"DarkMode_Explorer", NULL);
        }
        pView->Release();
      }
      CoTaskMemFree(pidl);
    }
  }

  RECT rcSys;
  GetClientRect(hwnd, &rcSys);
  PostMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rcSys.right, rcSys.bottom));
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void SetupTrayIcon(HWND hwnd) {
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = hwnd;
  nid.uID = g_uTrayID;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAYICON;
  nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"DeskManage");
  Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = hwnd;
  nid.uID = g_uTrayID;
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd);

  HMENU hMenu = CreatePopupMenu();
  AppendMenuW(hMenu, MF_STRING, IDM_NEW_BOX, L"New Box");
  AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
  AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

  TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
  DestroyMenu(hMenu);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    SetupTrayIcon(hwnd);
    return 0;
  case WM_TRAYICON:
    if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
      ShowTrayMenu(hwnd);
    }
    return 0;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDM_NEW_BOX) {
      CreateNewBox();
    } else if (LOWORD(wParam) == IDM_EXIT) {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    return 0;
  case WM_CLOSE:
    RemoveTrayIcon(hwnd);
    {
      auto boxesCopy = g_Boxes;
      for (auto &pair : boxesCopy) {
        SendMessage(pair.first, WM_CLOSE, 0, 0);
      }
    }
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
  SetProcessDpiAwareContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  g_hInst = hInstance;
  if (FAILED(OleInitialize(NULL)))
    return -1;

  WNDCLASSW wc = {0};
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = szMainClassName;
  RegisterClassW(&wc);

  WNDCLASSW wcBox = {0};
  wcBox.lpfnWndProc = BoxWndProc;
  wcBox.hInstance = hInstance;
  wcBox.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcBox.lpszClassName = szBoxClassName;
  RegisterClassW(&wcBox);

  g_hMainWnd = CreateWindowExW(0, szMainClassName, L"DeskManageMain", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
  
  g_hMsgHook = SetWindowsHookExW(WH_GETMESSAGE, GetMsgProc, NULL, GetCurrentThreadId());
  
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (g_hMsgHook) UnhookWindowsHookEx(g_hMsgHook);
  OleUninitialize();
  return 0;
}
