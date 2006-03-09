//
// Copyright (C) 2004-2005 Martin Wickett
//

#include <windows.h>
#include <stdio.h>

#include "resource.h"

#define snprintf _snprintf

//
// some global data
//
HWND ghWnd;
NOTIFYICONDATA nid;
HINSTANCE hAppInstance;

static const UINT WM_TRAY_NOTIFY = ( WM_APP + 1000 );
static const char szAppName[] = "SeamlessRDP Shell";

typedef void ( *SetHooksProc ) ();
typedef void ( *RemoveHooksProc ) ();
typedef int ( *GetInstanceCountProc ) ();

//
// spawn a message box
//
void Message( const char *message )
{
    MessageBox( GetDesktopWindow(), message, "SeamlessRDP Shell", MB_OK );
}

//
// manage the tray icon
//
bool InitTrayIcon()
{
    nid.cbSize = sizeof( NOTIFYICONDATA );
    nid.hWnd = ghWnd;
    nid.uID = 0;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_NOTIFY;
    strcpy( nid.szTip, szAppName );
    nid.hIcon = ::LoadIcon( hAppInstance, MAKEINTRESOURCE( IDI_TRAY ) );

    if ( Shell_NotifyIcon( NIM_ADD, &nid ) != TRUE ) {
        Message( "Unable to create tray icon." );
        return false;
    }

    return true;
}

//
// Remove tray icon
//
bool RemoveTrayIcon()
{
    if ( Shell_NotifyIcon( NIM_DELETE, &nid ) != TRUE ) {
        Message( "Unable to remove tray icon." );
        return false;
    }

    return true;

}

//
// manage the about dialog box
//
BOOL CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam,
                          LPARAM lParam )
{
    if ( uMsg == WM_COMMAND ) {
        WORD wID = LOWORD( wParam );
        if ( wID == IDOK )
            DestroyWindow( hwndDlg );
    }

    return 0;
}

void AboutDlg()
{
    DialogBox( hAppInstance, MAKEINTRESOURCE( IDD_ABOUT ), NULL, DialogProc );
}

//
// manage the context menu
//
void DoContextMenu()
{
    HMENU hMenu = LoadMenu( hAppInstance, MAKEINTRESOURCE( IDR_TRAY ) );
    if ( hMenu == NULL ) {
        Message( "Unable to load menu ressource." );
        return ;
    }

    HMENU hSubMenu = GetSubMenu( hMenu, 0 );
    if ( hSubMenu == NULL ) {
        Message( "Unable to find popup mennu." );
        return ;
    }

    // get the cursor position
    POINT pt;
    GetCursorPos( &pt );

    SetForegroundWindow( ghWnd );
    int cmd = TrackPopupMenu( hSubMenu,
                              TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, ghWnd, NULL );
    DestroyMenu( hMenu );

    switch ( cmd ) {
    case ID_WMEXIT: {
            PostQuitMessage( 0 );
            break;
        }
    case ID_WMABOUT: {
            AboutDlg();
            break;
        }
    }
}

//
// manage the main window
//
LONG WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch ( uMsg ) {
    case WM_DESTROY: {
            PostQuitMessage( 0 );
            return 0;
        }
    case WM_TRAY_NOTIFY: {
            if ( lParam == WM_RBUTTONDOWN )
                DoContextMenu();
            return 0;
        }
    }

    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

//
//Init window
//
bool InitWindow()
{
    // register the frame class
    WNDCLASS wndclass;
    wndclass.style = 0;
    wndclass.lpfnWndProc = ( WNDPROC ) MainWndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hAppInstance;
    wndclass.hIcon = 0;
    wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = ( HBRUSH ) ( COLOR_WINDOW + 1 );
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;

    if ( !RegisterClass( &wndclass ) ) {
        Message( "Unable to register the window class." );
        return false;
    }

    // create the frame
    ghWnd = CreateWindow( szAppName, szAppName,
                          WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |
                          WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 640,
                          480, NULL, NULL, hAppInstance, NULL );

    // make sure window was created
    if ( !ghWnd ) {
        Message( "Unable to create the window." );
        return false;
    }

    return true;
}

//
// our main loop
//
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow )
{
    HMODULE hHookDLL;
    GetInstanceCountProc pfnInstanceCount;
    SetHooksProc pfnSetHooks;
    RemoveHooksProc pfnRemoveHooks;

    hAppInstance = hInstance;

    hHookDLL = LoadLibrary( "hookdll.dll" );
    if ( !hHookDLL ) {
        Message( "Could not load hook DLL. Unable to continue." );
        return -1;
    }

    pfnInstanceCount = (GetInstanceCountProc) GetProcAddress( hHookDLL, "GetInstanceCount" );
    pfnSetHooks = (SetHooksProc) GetProcAddress( hHookDLL, "SetHooks" );
    pfnRemoveHooks = (RemoveHooksProc) GetProcAddress( hHookDLL, "RemoveHooks" );

    if ( !pfnInstanceCount || !pfnSetHooks || !pfnRemoveHooks ) {
        FreeLibrary( hHookDLL );
        Message( "Hook DLL doesn't contain the correct functions. Unable to continue." );
        return -1;
    }

    /* Check if the DLL is already loaded */
    if ( pfnInstanceCount() != 1 ) {
        FreeLibrary( hHookDLL );
        Message( "Another running instance of Seamless RDP detected." );
        return -1;
    }

    pfnSetHooks();

    // if we have been specified an app to launch, we will wait until the app has closed and use that for
    // our cue to exit
    if ( strlen( lpCmdLine ) > 0 ) {
        // Because we do not have a explorer.exe we need to make this application the replacement
        // shell. We do this by calling SystemParametersInfo. If we don't do this, we won't get the WH_SHELL notifications.

        // From MSDN:
        // Note that custom shell applications do not receive WH_SHELL messages. Therefore, any application that
        // registers itself as the default shell must call the SystemParametersInfo function with SPI_SETMINIMIZEDMETRICS
        // before it (or any other application) can receive WH_SHELL messages.

        MINIMIZEDMETRICS mmm;
        mmm.cbSize = sizeof( MINIMIZEDMETRICS );
        SystemParametersInfo( SPI_SETMINIMIZEDMETRICS,
                              sizeof( MINIMIZEDMETRICS ), &mmm, 0 );

        // We require DragFullWindows
        SystemParametersInfo( SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0 );

        //set the current directory to that of the requested app .exe location
        //tokenise lpCmdLine. first is the exe path. second (if exists) is the current directory to set.
        //SetCurrentDirectory ();

        //start process specified from command line arg.
        PROCESS_INFORMATION procInfo;
        STARTUPINFO startupInfo = {
                                      0
                                  };
        startupInfo.cb = sizeof( STARTUPINFO );
        char attr[] = "";
        LPTSTR process = lpCmdLine;
        DWORD dwExitCode;

        BOOL m_create =
            CreateProcess( NULL, process, NULL, NULL, FALSE, 0, NULL, NULL,
                           &startupInfo, &procInfo );

        if ( m_create != false ) {
            // A loop to watch the process.
            GetExitCodeProcess( procInfo.hProcess, &dwExitCode );

            while ( dwExitCode == STILL_ACTIVE ) {
                GetExitCodeProcess( procInfo.hProcess, &dwExitCode );
                Sleep( 1000 );
            }

            // Release handles
            CloseHandle( procInfo.hProcess );
            CloseHandle( procInfo.hThread );
        } else {
            // CreateProcess failed.
            char msg[ 256 ];
            snprintf( msg, sizeof( msg ), "Unable to launch the requested application:\n%s", process );
            Message( msg );
        }
    } else
        // we are launching without an app, therefore we will show the system tray app and wait for the user to close it
    {
        // create a dummy window to receive WM_QUIT message
        InitWindow();

        // create the tray icon
        InitTrayIcon();

        // just get and dispatch messages until we're killed
        MSG msg;
        while ( GetMessage( &msg, 0, 0, 0 ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        };

        // remove our tray icon
        RemoveTrayIcon();
    }


    // remove hook before saying goodbye
    pfnRemoveHooks();

    FreeLibrary( hHookDLL );

    return 1;
}
