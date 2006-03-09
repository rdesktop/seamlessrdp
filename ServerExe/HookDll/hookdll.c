//
// Copyright (C) 2004-2005 Martin Wickett
//

#include "hookdll.h"
#include <windows.h>
#include <winuser.h>
#include <stdio.h>
#include <stdarg.h>

#include "wtsapi32.h"
#include "cchannel.h"

#define DLL_EXPORT __declspec(dllexport)

// Shared DATA
#pragma data_seg ( "SHAREDDATA" )

// this is the total number of processes this dll is currently attached to
int iInstanceCount = 0;
HWND hWnd = 0;

#pragma data_seg ()

#pragma comment(linker, "/section:SHAREDDATA,rws")

#define snprintf _snprintf

HHOOK hCbtProc = 0;
HHOOK hShellProc = 0;
HHOOK hWndProc = 0;
HINSTANCE hInst = 0;
HANDLE m_vcHandle = 0;
HANDLE hMutex = 0;

void SendDebug( char *format, ... )
{
    va_list argp;
    char buf [ 256 ];

    sprintf( buf, "DEBUG1," );

    va_start( argp, format );
    _vsnprintf( buf + sizeof( "DEBUG1," ) - 1,
               sizeof( buf ) - sizeof( "DEBUG1," ) + 1, format, argp );
    va_end( argp );

    WriteToChannel( buf );
}



BOOL APIENTRY DllMain( HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpReserved )
{
    switch ( ul_reason_for_call ) {
    case DLL_PROCESS_ATTACH:
        // remember our instance handle
        hInst = hinstDLL;

        hMutex = CreateMutex( NULL, FALSE, "Local\\Seamless" );
        if (!hMutex)
            return FALSE;

        WaitForSingleObject( hMutex, INFINITE );
        ++iInstanceCount;
        ReleaseMutex( hMutex );

        OpenVirtualChannel();

        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        WaitForSingleObject( hMutex, INFINITE );
        --iInstanceCount;
        ReleaseMutex( hMutex );

        CloseVirtualChannel();

        CloseHandle( hMutex );

        break;
    }

    return TRUE;
}

LRESULT CALLBACK CallWndProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    HWND windowHandle2 = NULL;
    char result[ 255 ] = { ""
                         };
    CWPSTRUCT *details = ( CWPSTRUCT * ) lParam;
    CREATESTRUCT *cs = ( CREATESTRUCT * ) details->lParam;
    LONG dwStyle = GetWindowLong( details->hwnd, GWL_STYLE );
    WINDOWPOS *wp = ( WINDOWPOS * ) details->lParam;
    RECT rect;

    if ( nCode < 0 ) {
        return CallNextHookEx( hWndProc, nCode, wParam, lParam );
    }

    switch ( details->message ) {

    case WM_WINDOWPOSCHANGED:
        if ( dwStyle & WS_CHILD)
            break;


        if ( wp->flags & SWP_SHOWWINDOW ) {
            // FIXME: Now, just like create!
            SendDebug("SWP_SHOWWINDOW for %p!", details->hwnd);
            WriteToChannel( "CREATE1,0x%p,0x%x", details->hwnd, 0 );

            // FIXME: SETSTATE

            if ( !GetWindowRect( details->hwnd, &rect ) ) {
                SendDebug( "GetWindowRect failed!\n" );
                break;
            }
            WriteToChannel( "POSITION1,0x%p,%d,%d,%d,%d,0x%x",
                            details->hwnd,
                            rect.left, rect.top,
                            rect.right - rect.left,
                            rect.bottom - rect.top,
                            0 );
        }


        if ( wp->flags & SWP_HIDEWINDOW )
            WriteToChannel( "DESTROY1,0x%p,0x%x", details->hwnd, 0 );


        if ( !( dwStyle & WS_VISIBLE ) )
            break;

        if ( wp->flags & SWP_NOMOVE && wp->flags & SWP_NOSIZE )
            break;

        if ( !GetWindowRect( details->hwnd, &rect ) ) {
            SendDebug( "GetWindowRect failed!\n" );
            break;
        }

        WriteToChannel( "POSITION1,0x%p,%d,%d,%d,%d,0x%x",
                        details->hwnd,
                        rect.left, rect.top,
                        rect.right - rect.left,
                        rect.bottom - rect.top,
                        0 );

        break;


        /* Note: WM_WINDOWPOSCHANGING/WM_WINDOWPOSCHANGED are
        strange. Sometimes, for example when bringing up the
        Notepad About dialog, only an WM_WINDOWPOSCHANGING is
        sent. In some other cases, for exmaple when opening
        Format->Text in Notepad, both events are sent. Also, for
        some reason, when closing the Notepad About dialog, an
        WM_WINDOWPOSCHANGING event is sent which looks just like
        the event that was sent when the About dialog was opened...  */
    case WM_WINDOWPOSCHANGING:
        if ( dwStyle & WS_CHILD)
            break;

        if ( !( dwStyle & WS_VISIBLE ) )
            break;

        if ( !( wp->flags & SWP_NOZORDER ) )
            WriteToChannel( "ZCHANGE1,0x%p,0x%p,0x%x",
                            details->hwnd,
                            wp->flags & SWP_NOACTIVATE ? wp->hwndInsertAfter : 0,
                            0 );

        break;




    case WM_DESTROY:
        if ( dwStyle & WS_CHILD)
            break;

        WriteToChannel( "DESTROY1,0x%p,0x%x", details->hwnd, 0 );

        break;


    default:
        break;
    }

    return CallNextHookEx( hWndProc, nCode, wParam, lParam );
}

LRESULT CALLBACK CbtProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    char result[ 255 ] = { ""
                         };

	if ( nCode < 0 ) {
        return CallNextHookEx( hCbtProc, nCode, wParam, lParam );
    }

	switch ( nCode ) {
    case HCBT_MINMAX:

        if ( ( LOWORD( lParam ) == SW_SHOWMINIMIZED )
                || ( LOWORD( lParam ) == SW_MINIMIZE ) ) {
            MessageBox( 0, "Minimizing windows is not allowed in this version. Sorry!", "SeamlessRDP", MB_OK );
            return 1;
        }

        GetWindowText( ( HWND ) wParam, windowTitle, 150 );

        WriteToChannel( "SETSTATE1,0x%p,%s,0x%x,0x%x",
                        ( HWND ) wParam,
                        windowTitle,
                        LOWORD( lParam ),
                        0 );
        break;


    default:
        break;
    }



    return CallNextHookEx( hCbtProc, nCode, wParam, lParam );
}


LRESULT CALLBACK ShellProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    char result[ 255 ] = { ""
                         };
    char strWindowId[ 25 ];
    LONG b, t, l, r;
    char strW[ 5 ];
    char strY[ 5 ];
    char strX[ 5 ];
    char strH[ 5 ];
    RECT rect;

    if ( nCode < 0 ) {
        return CallNextHookEx( hShellProc, nCode, wParam, lParam );
    }

    switch ( nCode ) {
    case HSHELL_WINDOWCREATED:

        //get window id
        windowHandle = ( HWND ) wParam;
        itoa( ( int ) windowHandle, strWindowId, 10 );

        //get coords
        GetWindowRect( windowHandle, &rect );
        b = rect.bottom;
        t = rect.top;
        l = rect.left;
        r = rect.right;
        ltoa( b - t, strH, 10 );
        ltoa( t, strY, 10 );
        ltoa( r - l, strW, 10 );
        ltoa( l, strX, 10 );

        //get name
        GetWindowText( windowHandle, windowTitle, 150 );

        ////setup return string
        strcat( result, "MSG=HSHELL_WINDOWCREATED;OP=0;" );
        strcat( result, "ID=" );
        strcat( result, strWindowId );
        strcat( result, ";" );
        strcat( result, "TITLE=" );
        strcat( result, windowTitle );
        strcat( result, ";" );
        strcat( result, "X=" );
        strcat( result, strX );
        strcat( result, ";" );
        strcat( result, "Y=" );
        strcat( result, strY );
        strcat( result, ";" );
        strcat( result, "H=" );
        strcat( result, strH );
        strcat( result, ";" );
        strcat( result, "W=" );
        strcat( result, strW );
        strcat( result, "." );
        WriteToChannel( result );
        break;

    case HSHELL_WINDOWDESTROYED:

        //get window id
        windowHandle = ( HWND ) wParam;
        itoa( ( int ) windowHandle, strWindowId, 10 );

        //get coords
        GetWindowRect( windowHandle, &rect );
        b = rect.bottom;
        t = rect.top;
        l = rect.left;
        r = rect.right;
        ltoa( b - t, strH, 10 );
        ltoa( t, strY, 10 );
        ltoa( r - l, strW, 10 );
        ltoa( l, strX, 10 );

        //get name
        GetWindowText( windowHandle, windowTitle, 150 );

        ////setup return string
        strcat( result, "MSG=HSHELL_WINDOWDESTROYED;OP=1;" );
        strcat( result, "ID=" );
        strcat( result, strWindowId );
        strcat( result, ";" );
        strcat( result, "TITLE=" );
        strcat( result, windowTitle );
        strcat( result, ";" );
        strcat( result, "X=" );
        strcat( result, strX );
        strcat( result, ";" );
        strcat( result, "Y=" );
        strcat( result, strY );
        strcat( result, ";" );
        strcat( result, "H=" );
        strcat( result, strH );
        strcat( result, ";" );
        strcat( result, "W=" );
        strcat( result, strW );
        strcat( result, "." );
        WriteToChannel( result );
        break;


    default:
        break;
    }


    return CallNextHookEx( hShellProc, nCode, wParam, lParam );
}

DLL_EXPORT void SetHooks( void )
{
    if ( !hCbtProc ) {
        hCbtProc = SetWindowsHookEx( WH_CBT, ( HOOKPROC ) CbtProc, hInst, ( DWORD ) NULL );
    }

#if 0
    if ( !hShellProc ) {
        hShellProc = SetWindowsHookEx( WH_SHELL, ( HOOKPROC ) ShellProc, hInst, ( DWORD ) NULL );
    }
#endif

    if ( !hWndProc ) {
        hWndProc = SetWindowsHookEx( WH_CALLWNDPROC, ( HOOKPROC ) CallWndProc, hInst, ( DWORD ) NULL );
    }
}

DLL_EXPORT void RemoveHooks( void )
{
    if ( hCbtProc ) {
        UnhookWindowsHookEx( hCbtProc );
    }

    if ( hShellProc ) {
        UnhookWindowsHookEx( hShellProc );
    }

    if ( hWndProc ) {
        UnhookWindowsHookEx( hWndProc );
    }
}

DLL_EXPORT int GetInstanceCount()
{
    return iInstanceCount;
}

int OpenVirtualChannel()
{
    m_vcHandle = WTSVirtualChannelOpen( WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, CHANNELNAME );

    if ( m_vcHandle == NULL ) {
        return 0;
    } else {
        return 1;
    }
}

int CloseVirtualChannel()
{
    BOOL result = WTSVirtualChannelClose( m_vcHandle );

    m_vcHandle = NULL;

    if ( result ) {
        return 1;
    } else {
        return 0;
    }
}

int ChannelIsOpen()
{
    if ( m_vcHandle == NULL ) {
        return 0;
    } else {
        return 1;
    }
}

int WriteToChannel( char *format, ... )
{
    BOOL result;
    va_list argp;
    char buf [ 1024 ];
    int size;
    PULONG bytesRead = 0;
    PULONG pBytesWritten = 0;

    if ( !ChannelIsOpen() )
        return 1;

    va_start( argp, format );
    size = _vsnprintf( buf, sizeof( buf ), format, argp );
    va_end( argp );

    if ( size >= sizeof( buf ) )
        return 0;

    WaitForSingleObject( hMutex, INFINITE );
    result = WTSVirtualChannelWrite( m_vcHandle, buf, ( ULONG ) strlen( buf ), pBytesWritten );
    result = WTSVirtualChannelWrite( m_vcHandle, "\n", ( ULONG ) 1, pBytesWritten );
    ReleaseMutex( hMutex );

    if ( result ) {
        return 1;
    } else {
        return 0;
    }
}
