#include "pch.h"
#include "Richedit.h"
#include "Version.h"
#include <fstream>

#include "Memory.h"
#include "Random.h"
#include "Randomizer.h"
#include "Panels_.h"

#define HEARTBEAT 0x401
#define RANDOMIZE_READY 0x402
#define RANDOMIZING 0403
#define RANDOMIZE_DONE 0x404

/* ------- Temp ------- */
#include "PuzzleSerializer.h"
#include <sstream>
#include <iomanip>


HWND g_panelId;
Puzzle g_puzzle;

HWND g_rngDebug;
#define TMP5 0x505
/* ------- Temp ------- */

// Globals
HWND g_hwnd;
//HWND g_seed;
HWND g_randomizerStatus;
HINSTANCE g_hInstance;
auto g_witnessProc = std::make_shared<Memory>(L"witness64_d3d11.exe");
std::shared_ptr<Randomizer> g_randomizer;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_DESTROY) {
		PostQuitMessage(0);
    } else if (message == WM_NOTIFY) {
        MSGFILTER* m = (MSGFILTER *)lParam;
        if (m->msg == WM_KEYDOWN && m->wParam == VK_RETURN) {
            if (IsWindowEnabled(g_randomizerStatus) == TRUE) {
                PostMessage(g_hwnd, WM_COMMAND, RANDOMIZING, NULL);
                return 1; // Non-zero to indicate that message was handled
            }
        }
    } else if (message == WM_COMMAND || message == WM_TIMER || message == WM_NOTIFY) {
        switch (LOWORD(wParam)) {
            case HEARTBEAT:
                switch ((ProcStatus)lParam) {
                    case ProcStatus::NotRunning:
                        // Shut down randomizer, wait for startup
                        if (g_randomizer) {
                            g_randomizer = nullptr;
                            EnableWindow(g_randomizerStatus, FALSE);
                        }
                        break;
                    case ProcStatus::Running:
                        if (!g_randomizer) {
                            g_randomizer = std::make_shared<Randomizer>(g_witnessProc);
                            PostMessage(g_hwnd, WM_COMMAND, RANDOMIZE_READY, NULL);
                        }
                        break;
                    case ProcStatus::NewGame: // This status will fire only once per new game
                        //SetWindowText(g_seed, L"");
                        PostMessage(g_hwnd, WM_COMMAND, RANDOMIZE_READY, NULL);
                        break;
                    }
                break;
            case RANDOMIZE_READY:
                EnableWindow(g_randomizerStatus, TRUE);
                SetWindowText(g_randomizerStatus, L"Tutorialise");
                break;
            case RANDOMIZING:
                if (!g_randomizer) break; // E.g. an enter press at the wrong time
                EnableWindow(g_randomizerStatus, FALSE);

                std::thread([]{
                    SetWindowText(g_randomizerStatus, L"Tutorialising...");
                    g_randomizer->Randomize();
                    PostMessage(g_hwnd, WM_COMMAND, RANDOMIZE_DONE, NULL);
                }).detach();
                break;
            case RANDOMIZE_DONE:
                EnableWindow(g_randomizerStatus, FALSE);
                SetWindowText(g_randomizerStatus, L"Tutorialised!");
                break;

        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

HWND CreateLabel(int x, int y, int width, LPCWSTR text) {
	return CreateWindow(L"STATIC", text,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | SS_LEFT,
		x, y, width, 16, g_hwnd, NULL, g_hInstance, NULL);
}

HWND CreateMultiLabel(int x, int y, int width, int height, LPCWSTR text) {
    return CreateWindow(L"STATIC", text,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | SS_LEFT,
        x, y, width, height, g_hwnd, NULL, g_hInstance, NULL);
}

HWND CreateText(int x, int y, int width, LPCWSTR defaultText = L"") {
	return CreateWindow(MSFTEDIT_CLASS, defaultText,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER,
        x, y, width, 26, g_hwnd, NULL, g_hInstance, NULL);
}

#pragma warning(push)
#pragma warning(disable: 4312)
HWND CreateButton(int x, int y, int width, LPCWSTR text, int message) {
	return CreateWindow(L"BUTTON", text,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		x, y, width, 26, g_hwnd, (HMENU)message, g_hInstance, NULL);
}

HWND CreateCheckbox(int x, int y, int message) {
	return CreateWindow(L"BUTTON", L"",
        WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
        x, y, 12, 12, g_hwnd, (HMENU)message, g_hInstance, NULL);
}
#pragma warning(pop)

int findGlobals() {
        int address = 0;
        for (int i = 0x620000; i < 0x660000; i += 4) {
            Memory::GLOBALS = i;
            try {
                if ((address = g_witnessProc->ReadPanelData<int>(0x17E52, STYLE_FLAGS, 1)[0]) == 0x0000A040) {
                    return i;
                }
            }
            catch (std::exception) {}
        }
        for (int i = 0x600000; i < 0x620000; i += 4) {
            Memory::GLOBALS = i;
            try {
                if ((address = g_witnessProc->ReadPanelData<int>(0x17E52, STYLE_FLAGS, 1)[0]) == 0x0000A040) {
                    return i;
                }
            }
            catch (std::exception) {}
        }
        return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
	LoadLibrary(L"Msftedit.dll");
	WNDCLASSW wndClass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		NULL,
		LoadCursor(nullptr, IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW+1),
		WINDOW_CLASS,
		WINDOW_CLASS,
	};
	RegisterClassW(&wndClass);

    //Initialize memory globals constant depending on game version
    for (int g : Memory::globalsTests) {
        try {
            Memory::GLOBALS = g;
            if (g_witnessProc->ReadPanelData<int>(0x17E52, STYLE_FLAGS) != 0xA040) throw std::exception();
            break;
        }
        catch (std::exception) { Memory::GLOBALS = 0; }
    }
    if (!Memory::GLOBALS) {
        std::ifstream file("globals.txt");
        if (file.is_open()) {
            file >> std::hex >> Memory::GLOBALS;
        }
        else {
            std::string str = "Globals ptr not found. Press OK to search for globals ptr. It will probably take around 5 minutes. This popup will close and the calculation will run in the background. Please keep The Witness open during this time.";
            if (MessageBox(GetActiveWindow(), std::wstring(str.begin(), str.end()).c_str(), NULL, MB_OK) != IDOK) return 0;
            int address = findGlobals();
            if (address) {
                std::stringstream ss; ss << std::hex << "Address found: 0x" << address << ". This address wil be automatically loaded next time. Please post an issue on Github with this address so that it can be added in the future.";
                std::string str = ss.str();
                MessageBox(GetActiveWindow(), std::wstring(str.begin(), str.end()).c_str(), NULL, MB_OK);
                std::ofstream ofile("globals.txt", std::ofstream::app);
                ofile << std::hex << address << std::endl;
            }
            else {
                str = "Address could not be found. Please post an issue on the Github page.";
                MessageBox(GetActiveWindow(), std::wstring(str.begin(), str.end()).c_str(), NULL, MB_OK);
                return 0;
            }
        }
    }


    g_hInstance = hInstance;

	RECT rect;
    GetClientRect(GetDesktopWindow(), &rect);
	g_hwnd = CreateWindow(WINDOW_CLASS, PRODUCT_NAME, WS_OVERLAPPEDWINDOW,
      rect.right - 550, 200, 500, 180, nullptr, nullptr, hInstance, nullptr);

    CreateMultiLabel(10, 10, 460, 86, L"This mod replaces most puzzles in the game with Tutorial Straight (the first puzzle in the tunnel where you start the game). Certain special panels are unaffected. Additionally, some panels (e.g. the tutorial gate, and every puzzle in Bunker) behave a little strangely now, and can be solved by simply double clicking in the middle of the panel.");
    CreateLabel(390, 110, 90, L"Version: " VERSION_STR);
    g_randomizerStatus = CreateButton(120, 105, 180, L"Tutorialise", RANDOMIZING);
    EnableWindow(g_randomizerStatus, FALSE);

    g_witnessProc->StartHeartbeat(g_hwnd, HEARTBEAT);

	ShowWindow(g_hwnd, nCmdShow);
	UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) == TRUE) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}
