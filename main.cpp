#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <string>
#include <shellapi.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

const std::wstring DESKTOP_PATH = L"C:\\Users\\xm\\Desktop\\";
const std::wstring BOX_PATH = L"C:\\Users\\xm\\Desktop\\DesktopBox_1\\";

IExplorerBrowser* g_peb = nullptr;
int g_alpha = 200;

void MoveFilesBackAndClean() {
    WCHAR src[MAX_PATH + 2] = {0};
    wcscpy_s(src, BOX_PATH.c_str());
    src[wcslen(src)] = L'*'; 

    WCHAR dst[MAX_PATH + 2] = {0};
    wcscpy_s(dst, DESKTOP_PATH.c_str());

    SHFILEOPSTRUCTW sfo = {0};
    sfo.wFunc = FO_MOVE;
    sfo.pFrom = src;
    sfo.pTo = dst;
    sfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    SHFileOperationW(&sfo);

    RemoveDirectoryW(BOX_PATH.c_str());
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            CreateDirectoryW(BOX_PATH.c_str(), NULL);

            if (SUCCEEDED(CoCreateInstance(CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_peb)))) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                FOLDERSETTINGS fs = {0};
                fs.ViewMode = FVM_ICON;
                fs.fFlags = FWF_AUTOARRANGE | FWF_NOWEBVIEW | FWF_HIDEFILENAMES | FWF_TRANSPARENT;
                
                g_peb->Initialize(hwnd, &rc, &fs);
                g_peb->SetOptions(EBO_NAVIGATEONCE);
                
                PIDLIST_ABSOLUTE pidl;
                if (SUCCEEDED(SHParseDisplayName(BOX_PATH.c_str(), NULL, &pidl, 0, NULL))) {
                    g_peb->BrowseToIDList(pidl, SBSP_ABSOLUTE);
                    ILFree(pidl);
                }
            }
            break;
        }
        case WM_SIZE: {
            if (g_peb) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                g_peb->SetRect(NULL, rc);
            }
            break;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
            if (hit == HTCLIENT) {
                POINT pt;
                pt.x = GET_X_LPARAM(lParam);
                pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hwnd, &pt);
                
                RECT rc;
                GetClientRect(hwnd, &rc);
                int border = 10;
                
                if (pt.x < border && pt.y < border) return HTTOPLEFT;
                if (pt.x > rc.right - border && pt.y < border) return HTTOPRIGHT;
                if (pt.x < border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
                if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;
                if (pt.x < border) return HTLEFT;
                if (pt.x > rc.right - border) return HTRIGHT;
                if (pt.y < border) return HTTOP;
                if (pt.y > rc.bottom - border) return HTBOTTOM;
                
                return HTCAPTION; 
            }
            return hit;
        }
        case WM_MOUSEWHEEL: {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0 && g_alpha < 255) g_alpha += 15;
            else if (zDelta < 0 && g_alpha > 50) g_alpha -= 15;
            if (g_alpha > 255) g_alpha = 255;
            if (g_alpha < 50) g_alpha = 50;
            SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);
            break;
        }
        case WM_DESTROY: {
            if (g_peb) {
                g_peb->Destroy();
                g_peb->Release();
            }
            MoveFilesBackAndClean();
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"DesktopBoxClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED, 
        L"DesktopBoxClass", L"DesktopBox", 
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME,
        200, 200, 400, 300, NULL, NULL, hInst, NULL);
        
    SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}
