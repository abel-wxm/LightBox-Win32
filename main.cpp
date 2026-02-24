#include <windows.h>
#include <shellapi.h>
#include <string>

// 锁定你的 C 盘收纳盒路径
const char* TARGET_PATH = "C:\\Users\\xm\\Desktop\\DesktopBox\\";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateDirectory(TARGET_PATH, NULL);
            DragAcceptFiles(hwnd, TRUE);
            break;
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            char srcPath[MAX_PATH];
            UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < count; i++) {
                DragQueryFile(hDrop, i, srcPath, MAX_PATH);
                std::string name = srcPath;
                name = name.substr(name.find_last_of("\\") + 1);
                std::string dest = std::string(TARGET_PATH) + name;
                MoveFile(srcPath, dest.c_str());
            }
            DragFinish(hDrop);
            ShellExecute(NULL, "open", TARGET_PATH, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case WM_NCHITTEST: return HTCAPTION; // 全窗口可拖动
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrush = CreateSolidBrush(RGB(20, 20, 20)); // 深黑色背景
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);
            SetTextColor(hdc, RGB(0, 255, 0)); // 绿色文字提示
            SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, 15, 15, "Drop Files Here to Organize", 27);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    const char* CLASS_NAME = "LightBoxClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // 创建一个 300x400 的固定窗口
    HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, CLASS_NAME, 
                               "LightBox", WS_POPUP | WS_VISIBLE, 
                               200, 200, 300, 400, NULL, NULL, hInst, NULL);
    SetLayeredWindowAttributes(hwnd, 0, 210, LWA_ALPHA); // 透明度 210/255
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
