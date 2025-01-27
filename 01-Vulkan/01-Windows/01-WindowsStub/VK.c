// header files
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "VK.h"

// macros
#define WIN_WIDTH  800
#define WIN_HEIGHT 600
#define WIN_TITLE  TEXT("VNR : Vulkan")

// global function declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// global variable declarations
HWND            ghwnd        = NULL;
BOOL            gbFullscreen = FALSE;
BOOL            gbActive     = FALSE;
WINDOWPLACEMENT wpPrev;
DWORD           dwStyle;

// for file IO
FILE *gpFILE = NULL;

// entry-point function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    // function prototypes
    int initialize(void);
    void display(void);
    void update(void);
    void uninitialize();

    // local variable declarations
    WNDCLASSEX wndclass    = { sizeof(WNDCLASSEX) };
    HWND       hwnd        = NULL;
    TCHAR      szAppName[] = TEXT("VNRWindow");
    BOOL       bDone       = FALSE;
    MSG        msg         = { 0 };

    // code
    // open the log file
    gpFILE = fopen("Log.txt", "w");
    if(gpFILE == NULL)
    {
        MessageBox(NULL, TEXT("fopen() failed to open the log file."), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(gpFILE, "%s() : Program started successfully.\n", __FUNCTION__);
    }

    // fill the window class
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpfnWndProc   = WndProc;
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName  = NULL;
    wndclass.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    // register the window class
    RegisterClassEx(&wndclass);

    // create the main window in memory
    hwnd = CreateWindowEx(
                            WS_EX_APPWINDOW, 
                            szAppName, 
                            WIN_TITLE, 
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                            (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2,
                            (GetSystemMetrics(SM_CYSCREEN) - WIN_HEIGHT) / 2,
                            WIN_WIDTH,
                            WIN_HEIGHT,
                            HWND_DESKTOP,
                            NULL,
                            hInstance,
                            NULL
                         );

    if(!hwnd)
    {
        MessageBox(NULL, TEXT("CreateWindowEx() failed."), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(EXIT_FAILURE);
    }

    // copy the local window handle into the global window handle
    ghwnd = hwnd;

    // initialization
    INT iResult = initialize();
    if(iResult != 0)
    {
        MessageBox(NULL, TEXT("initialize() failed."), TEXT("Error"), MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
    }
    else
    {
        fprintf(gpFILE, "%s() : initialize() succeeded.\n", __FUNCTION__);
    }

    // show the window
    ShowWindow(hwnd, iCmdShow);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // gameloop
    while(bDone == FALSE)
    {
        ZeroMemory(&msg, sizeof(MSG));
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
            {
                bDone = TRUE;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else if(gbActive)
        {
            // Render
            display();

            // Update
            update();
        }
    }

    // uninitialization
    uninitialize();
    
    // un-register the window class
    UnregisterClass(szAppName, hInstance);

    return((int)msg.wParam);
}

// callback function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    // function declarations
    void resize(int width, int height);
    void ToggleFullscreen(void);

    // code
    switch(iMsg)
    {
    case WM_CREATE:
        memset(&wpPrev, 0, sizeof(WINDOWPLACEMENT));
        wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;
    case WM_SETFOCUS:
        gbActive = TRUE;
        break;
    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;
    case WM_ERASEBKGND:
        return(0);
    case WM_SIZE:
        resize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_KEYDOWN:
        switch(LOWORD(wParam))
        {
        case VK_ESCAPE:
            DestroyWindow(hwnd);
            break;
        default:
            break;
        }
        break;
    case WM_CHAR:
        switch(LOWORD(wParam))
        {
        case 'F':
        case 'f':
            ToggleFullscreen();
            break;
        default:
            break;
        }
        break;     
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}

// toggle fullscreen
void ToggleFullscreen(void)
{
    // local variables
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };

    // code
    // if window isn't in fullscreen mode
    if(gbFullscreen == FALSE)
    {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if(dwStyle & WS_OVERLAPPEDWINDOW)
        {
            if(GetWindowPlacement(ghwnd, &wpPrev) && 
               GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &monitorInfo))
            {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(
                             ghwnd, 
                             HWND_TOP, 
                             monitorInfo.rcMonitor.left, 
                             monitorInfo.rcMonitor.top,
                             monitorInfo.rcMonitor.right  - monitorInfo.rcMonitor.left,
                             monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED
                            );

                ShowCursor(FALSE);

                gbFullscreen = TRUE;
            }
        }
    }
    else // window is already in fullscreen mode
    {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(
                     ghwnd, 
                     HWND_TOP, 
                     0, 
                     0, 
                     0, 
                     0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                    );
        
        ShowCursor(TRUE);

        gbFullscreen = FALSE;
    }
}

int initialize(void)
{
    // function declarations
    
    // code

    return(0);
}

void resize(int width, int height)
{
    // code
    if(height <= 0)
    {
        height = 1;
    }
}

void display(void)
{
    // code
}

void update(void)
{
    // code
}

void uninitialize(void)
{
    // function declarations
    void ToggleFullscreen(void);

    // code
    // if application is exitting in fullscreen
    if(gbFullscreen == TRUE)
    {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    // Destroy window
    if(ghwnd)
    {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    // close the log file
    if(gpFILE)
    {
        fprintf(gpFILE, "%s() : Program ended successfully.\n", __FUNCTION__);
        fclose(gpFILE);
        gpFILE = NULL;
    }
}
