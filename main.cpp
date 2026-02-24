#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma warning(disable: 4996)

const char* TARGET_PATH = "C:\\Users\\xm\\Desktop\\DesktopBox\\";

struct BoxItem {
    std::string name;
    HICON hIcon;
};
std::vector<BoxItem> g_items;
int g_width = 320;
int g_alpha = 200;

void RefreshBox(HWND hwnd) {
    for (size_t i = 0; i < g_items.size(); i++) DestroyIcon(g_items[i].hIcon);
    g_items.clear();

    WIN32_FIND_DATAA fd;
    std::string search = std::string(TARGET_PATH) + "*";
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                BoxItem item;
                item.name = fd.cFileName;
                std::string fullPath = std::string(TARGET_PATH) + fd.cFileName;
                
                SHFILEINFOA sfi = {0};
                SHGetFileInfoA(fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON);
                item.hIcon = sfi.hIcon;
                g_items.push_back(item);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateDirectoryA(TARGET_PATH, NULL);
            DragAcceptFiles(hwnd, TRUE);
            RefreshBox(hwnd);
            break;
            
        case WM_SIZE:
            g_width = LOWORD(lParam);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
            
        case WM_MOUSEWHEEL: {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0 && g_alpha < 255) g_alpha += 15;
            else if (zDelta < 0 && g_alpha > 50) g_alpha -= 15;
            if (g_alpha > 255) g_alpha = 255;
            if (g_alpha < 50) g_alpha = 50;
            SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);
            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < count; i++) {
                char src[MAX_PATH] = {0};
                DragQueryFileA(hDrop, i, src, MAX_PATH);
                
                char doubleNullSrc[MAX_PATH + 2] = {0};
                strcpy(doubleNullSrc, src);
                
                char doubleNullDst[MAX_PATH + 2] = {0};
                strcpy(doubleNullDst, TARGET_PATH);
                
                SHFILEOPSTRUCTA sfo = {0};
                sfo.hwnd = hwnd;
                sfo.wFunc = FO_MOVE;
                sfo.pFrom = doubleNullSrc;
                sfo.pTo = doubleNullDst;
                sfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
                SHFileOperationA(&sfo);
            }
            DragFinish(hDrop);
            RefreshBox(hwnd);
            break;
        }

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            int cols = max(1, g_width / 70);
            int col = mx / 70;
            int row = my / 90;
            int idx = row * cols + col;
            if (idx < 0 || idx >= (int)g_items.size()) {
                ReleaseCapture();
                SendMessage(hwnd, WM_SYSCOMMAND, 0xF012, 0);
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            int cols = max(1, g_width / 70);
            int col = mx / 70;
            int row = my / 90;
            int idx = row * cols + col;
            if (idx >= 0 && idx < (int)g_items.size()) {
                std::string path = std::string(TARGET_PATH) + g_items[idx].name;
                ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HBRUSH hBrush = CreateSolidBrush(RGB(25, 25, 25));
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            int cols = max(1, g_width / 70);
            for (size_t i = 0; i < g_items.size(); i++) {
                int col = i % cols;
                int row = i / cols;
                int x = col * 70 + 15;
                int y = row * 90 + 15;

                DrawIcon(hdc, x, y, g_items[i].hIcon);
                
                RECT rc = { x - 15, y + 35, x + 47, y + 75 };
                DrawTextA(hdc, g_items[i].name.c_str(), -1, &rc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
            
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.style = CS_DBLCLKS; 
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "LightBoxClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED, 
        "LightBoxClass", "LightBox", 
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME,
        200, 200, 320, 450, NULL, NULL, hInst, NULL);
        
    SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
