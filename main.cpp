// language: C++17, file: main.cpp, app: floorgazer — NFT floor price tracker
// Windows 11, MSVC, WinAPI + WinHTTP. No external dependencies.
// Shows floor prices for a watchlist of collections via the Reservoir API
// (free tier, no key). Refresh on demand.
#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

struct Collection { std::wstring name; const wchar_t* contract; double floor; };

// Watchlist — swap in any collection contract addresses you track.
static Collection g_watch[] = {
    { L"Azuki",                L"0xed5af388ebe3f0ad8fa0f4cf3a6721f3e4c5b19b", 0 },
    { L"Bored Ape Yacht Club", L"0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d", 0 },
};

static HWND g_list;

static std::string HttpsGet(const std::wstring& host, const std::wstring& path) {
    std::string out;
    HINTERNET sess = WinHttpOpen(L"floorgazer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) return out;
    HINTERNET conn = WinHttpConnect(sess, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
        if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                && WinHttpReceiveResponse(req, nullptr)) {
            DWORD bytes = 0; char buf[8192];
            for (;;) {
                if (!WinHttpReadData(req, buf, sizeof(buf), &bytes) || bytes == 0) break;
                out.append(buf, bytes);
            }
        }
        if (req) WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(sess);
    return out;
}

// Extract "floorAsk":{"price":<num> from the collection response.
static double ExtractFloor(const std::string& json) {
    size_t p = json.find("\"floorAsk\":{\"price\":");
    if (p == std::string::npos) return 0;
    p += 20;
    return std::stod(json.substr(p, json.find_first_of(",}", p) - p));
}

static DWORD WINAPI Fetch(LPVOID) {
    for (auto& c : g_watch) {
        std::wstring path = L"/collections/v7?id=" + std::wstring(c.contract);
        std::string body = HttpsGet(L"api.reservoir.tools", path);
        if (!body.empty()) c.floor = ExtractFloor(body);
        Sleep(300); // polite pacing
    }
    PostMessage(g_list, WM_APP, 0, 0);
    return 0;
}

static void FillList() {
    ListView_DeleteAllItems(g_list);
    LVITEMW it{}; it.mask = LVIF_TEXT;
    wchar_t buf[64];
    for (size_t i = 0; i < std::size(g_watch); ++i) {
        it.iItem = (int)i; it.pszText = (LPWSTR)g_watch[i].name.c_str();
        it.iItem = ListView_InsertItem(g_list, &it);
        swprintf(buf, 64, L"%.3f ETH", g_watch[i].floor);
        ListView_SetItemText(g_list, it.iItem, 1, buf);
        it.pszText = (LPWSTR)g_watch[i].contract;
        ListView_SetItemText(g_list, it.iItem, 2, it.pszText);
    }
}

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);
        CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      16, 16, 120, 32, w, (HMENU)1, nullptr, nullptr);
        g_list = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
                                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                 16, 60, 740, 300, w, (HMENU)2, nullptr, nullptr);
        LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH;
        const wchar_t* heads[] = { L"Collection", L"Floor", L"Contract" };
        int widths[] = { 240, 140, 340 };
        for (int i = 0; i < 3; ++i) {
            col.pszText = (LPWSTR)heads[i]; col.cx = widths[i];
            ListView_InsertColumn(g_list, i, &col);
        }
        CreateThread(nullptr, 0, Fetch, nullptr, 0, nullptr);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) CreateThread(nullptr, 0, Fetch, nullptr, 0, nullptr);
        return 0;
    case WM_APP: FillList(); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc; wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 23));
    wc.lpszClassName = L"FloorGazerWnd";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, L"FloorGazerWnd", L"FloorGazer — nft floor tracker",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 790, 420,
                             nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
