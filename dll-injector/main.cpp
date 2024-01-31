#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <TlHelp32.h>
#include <string>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx9.h>
#include <tchar.h>
#include <commdlg.h>
#include <d3d9.h>

// Function prototypes
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL InjectDLL(DWORD processId, const wchar_t* dllPath);
BOOL ListProcesses();
std::wstring GetProcessName(DWORD processID);

// Global variables
DWORD g_Processes[1024];
DWORD g_ProcessCount = 0;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static LPDIRECT3D9 d3dEngine = nullptr;
static LPDIRECT3DDEVICE9 d3dDevice = nullptr;
static UINT ResizeWidth = 0, ResizeHeight = 0;
static D3DPRESENT_PARAMETERS d3dParameters = {};
static ImGuiContext* g_ImGuiContext = nullptr;

// Main function
int main() {
    printf("Application started\n");

    // Create window
    HWND hWnd;
    {
        printf("Creating window\n");
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Sathariel's Stealth Injector"), NULL };
        RegisterClassEx(&wc);
        hWnd = CreateWindow(wc.lpszClassName, _T("Sathariel's Stealth Injector"), WS_OVERLAPPEDWINDOW, 100, 100, 640, 480, NULL, NULL, wc.hInstance, NULL);
        if (hWnd == NULL) {
            MessageBox(NULL, _T("Window creation failed!"), _T("Error"), MB_ICONERROR);
            return 1;
        }

    }

    // Initialize DirectX 9
    d3dEngine = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3dEngine == NULL) {
        MessageBox(hWnd, L"Failed to create DirectX 9 interface!", L"Error", MB_ICONERROR);
        return 1;
    }

    // Create a DirectX 9 device
    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dParameters = d3dpp;

    if (d3dEngine->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dParameters, &d3dDevice) < 0) {
        d3dEngine->Release();
        MessageBox(hWnd, L"Failed to create DirectX 9 device!", L"Error", MB_ICONERROR);
        return 1;
    }

    printf("DirectX initialized\n");

    ::ShowWindow(hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;  // Enable Multi-Viewport / Platform Windows
    g_ImGuiContext = ImGui::GetCurrentContext(); // Store the context pointer
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX9_Init(d3dDevice);

    printf("ImGui initialized\n");

    // List processes
    ListProcesses();

    printf("Message loop started\n");

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (ResizeWidth != 0 && ResizeHeight != 0)
        {
            d3dParameters.BackBufferWidth = ResizeWidth;
            d3dParameters.BackBufferHeight = ResizeHeight;
            ResizeWidth = ResizeHeight = 0;
            ImGui_ImplDX9_InvalidateDeviceObjects();
            HRESULT hr = d3dDevice->Reset(&d3dParameters);
            if (hr == D3DERR_INVALIDCALL)
                IM_ASSERT(0);
            ImGui_ImplDX9_CreateDeviceObjects();
        }

        // Start ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // UI
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("DLL Injector", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit"))
                    done = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Text("Select a process:");
        static int selectedProcess = 0;
        if (ImGui::ListBox("Processes", &selectedProcess, [](void* data, int idx, const char** out_text) {
            static char buffer[256]; // Adjust buffer size as needed
            std::wstring processName = GetProcessName(g_Processes[idx]);
            if (!processName.empty()) {
                WideCharToMultiByte(CP_UTF8, 0, processName.c_str(), -1, buffer, sizeof(buffer), NULL, NULL);
                sprintf(buffer + strlen(buffer), " - %lu", g_Processes[idx]);
                *out_text = buffer;
            }
            else {
                sprintf(buffer, "%lu", g_Processes[idx]);
                *out_text = buffer;
            }
            return true;
            }, nullptr, g_ProcessCount, 5)) {
            // Process selected, do something with the process ID (g_Processes[selectedProcess])
        }




        static wchar_t dllPath[MAX_PATH] = L"";
        
        

        
        if (ImGui::Button("Browse", ImVec2(ImGui::GetWindowContentRegionMax().x * 0.5f, 0))) {
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = dllPath;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(dllPath);
            ofn.lpstrFilter = L"DLL Files\0*.dll\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

            if (GetOpenFileName(&ofn)) {
                
            }
        }
        static char convertedPath[MAX_PATH] = "";
        WideCharToMultiByte(CP_UTF8, 0, dllPath, -1, convertedPath, MAX_PATH, NULL, NULL);
        ImGui::InputText("DLL Path", convertedPath, MAX_PATH);

        if (ImGui::Button("Inject", ImVec2(ImGui::GetWindowContentRegionMax().x, 0))) {
            if (selectedProcess != -1 && dllPath[0] != '\0') {
                if (InjectDLL(g_Processes[selectedProcess], reinterpret_cast<const wchar_t*>(dllPath))) {
                    MessageBox(hWnd, L"DLL injected successfully!", L"Success", MB_ICONINFORMATION);
                }
                else {
                    MessageBox(hWnd, L"Failed to inject DLL!", L"Error", MB_ICONERROR);
                }
            }
            else {
                MessageBox(hWnd, L"Process ID or DLL path is invalid!", L"Error", MB_ICONERROR);
            }
        }
        ImGui::End();

        // Rendering
        d3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);
        if (d3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            d3dDevice->EndScene();
        }
        d3dDevice->Present(NULL, NULL, NULL, NULL);
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    d3dDevice->Release();
    d3dEngine->Release();
    DestroyWindow(hWnd);
    UnregisterClass(_T("Sathariel's Stealth Injector"), GetModuleHandle(NULL));

    return 0;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            ResizeWidth = (UINT)LOWORD(lParam);
            ResizeHeight = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) 
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_MOVE:
       
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGuiIO& io = ImGui::GetIO();
            RECT rect;
            GetWindowRect(hWnd, &rect);
            io.DisplaySize.x = (float)(rect.right - rect.left);
            io.DisplaySize.y = (float)(rect.bottom - rect.top);
        }
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// DLL injection function
BOOL InjectDLL(DWORD processId, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == nullptr)
        return FALSE;

    LPVOID pLoadLibraryW = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW"));
    if (pLoadLibraryW == nullptr) {
        CloseHandle(hProcess);
        return FALSE;
    }

    LPVOID pRemoteBuffer = VirtualAllocEx(hProcess, nullptr, wcslen(dllPath) * sizeof(wchar_t) + sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemoteBuffer == nullptr) {
        CloseHandle(hProcess);
        return FALSE;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuffer, dllPath, wcslen(dllPath) * sizeof(wchar_t) + sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    HANDLE hRemoteThread = CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW), pRemoteBuffer, 0, nullptr);
    if (hRemoteThread == nullptr) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    WaitForSingleObject(hRemoteThread, INFINITE);
    CloseHandle(hRemoteThread);
    VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return TRUE;
}

// List processes function
BOOL ListProcesses() {
    DWORD processes[1024];
    DWORD bytesReturned = 0;
    if (!EnumProcesses(processes, sizeof(processes), &bytesReturned))
        return FALSE;

    g_ProcessCount = bytesReturned / sizeof(DWORD);
    for (DWORD i = 0; i < g_ProcessCount; i++)
        g_Processes[i] = processes[i];

    return TRUE;
}


std::wstring GetProcessName(DWORD processId) {
    std::wstring processName;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess != nullptr) {
        wchar_t buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer) / sizeof(buffer[0]);
        if (QueryFullProcessImageName(hProcess, 0, buffer, &bufferSize)) {
            std::wstring fullPath(buffer);
            size_t pos = fullPath.find_last_of(L"\\");
            if (pos != std::wstring::npos) {
                processName = fullPath.substr(pos + 1);
            }
        }
        CloseHandle(hProcess);
    }

    return processName;
}