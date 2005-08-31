//
// Copyright (C) 2004-2005 Martin Wickett
//

#include "hookdll.h"
#include <windows.h>
#include <winuser.h>
#include <stdio.h>
#include <stdarg.h>

#include "wtsapi32.h"
#include "Cchannel.h"

#define DLL_EXPORT extern "C" __declspec(dllexport)

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


void SendDebug( char *format, ... )
{
    va_list argp;
    char buf [ 256 ];
    
    va_start( argp, format );
    vsprintf( buf, format, argp );
    va_end( argp );
    
    WriteToChannel( "DEBUG1," );
    WriteToChannel( buf );
    WriteToChannel( "\n" );
}



BOOL APIENTRY DllMain( HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpReserved )
{
    switch ( ul_reason_for_call ) {
        case DLL_PROCESS_ATTACH: {
            // remember our instance handle
            hInst = hinstDLL;
            ++iInstanceCount;
            OpenVirtualChannel();
            break;
        }
        
        case DLL_THREAD_ATTACH:
        break;
        case DLL_THREAD_DETACH:
        break;
        case DLL_PROCESS_DETACH: {
            --iInstanceCount;
            CloseVirtualChannel();
        }
        break;
    }
    
    return TRUE;
}

LRESULT CALLBACK CallWndProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if ( nCode < 0 ) {
        return CallNextHookEx( hWndProc, nCode, wParam, lParam );
    }
    
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
    RECT *rect = ( RECT * ) details->lParam;
    
    switch ( details->message ) {
    
        case WM_SIZING:
        case WM_MOVING:
        if ( !( dwStyle & WS_VISIBLE ) )
            break;
            
        if ( !( dwStyle & WS_DLGFRAME ) )
            break;
            
        snprintf( result, sizeof( result ),
                  "POSITION1,0x%p,%d,%d,%d,%d,0x%x\n",
                  details->hwnd,
                  rect->left, rect->top,
                  rect->right - rect->left,
                  rect->bottom - rect->top,
                  0 );
        result[ sizeof( result ) - 1 ] = '\0';
        WriteToChannel( result );
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
        
        if ( !( dwStyle & WS_VISIBLE ) )
            break;
            
        if ( !( dwStyle & WS_DLGFRAME ) )
            break;
            
        if ( !( wp->flags & SWP_NOZORDER ) ) {
            snprintf( result, sizeof( result ),
                      "ZCHANGE1,0x%p,0x%p,0x%x\n",
                      details->hwnd,
                      wp->flags & SWP_NOACTIVATE ? wp->hwndInsertAfter : 0,
                      0 );
            result[ sizeof( result ) - 1 ] = '\0';
            WriteToChannel( result );
        }
        break;
        
        
        case WM_CREATE:
        if ( cs->style & WS_DLGFRAME ) {
        
            snprintf( result, sizeof( result ),
                      "CREATE1,0x%p,0x%x\n",
                      details->hwnd, 0 );
            result[ sizeof( result ) - 1 ] = '\0';
            WriteToChannel( result );
            
            snprintf( result, sizeof( result ),
                      "SETSTATE1,0x%p,%s,0x%x,0x%x\n",
                      details->hwnd,
                      cs->lpszName,
                      1,      // FIXME: Check for WS_MAXIMIZE/WS_MINIMIZE
                      0 );
            result[ sizeof( result ) - 1 ] = '\0';
            WriteToChannel( result );
            
            snprintf( result, sizeof( result ),
                      "POSITION1,0x%p,%d,%d,%d,%d,0x%x\n",
                      details->hwnd,
                      cs->x,
                      cs->y,
                      cs->cx,
                      cs->cy,
                      0 );
            result[ sizeof( result ) - 1 ] = '\0';
            WriteToChannel( result );
            
        }
        break;
        
        
        case WM_DESTROY:
        if ( dwStyle & WS_DLGFRAME ) {
            snprintf( result, sizeof( result ),
                      "DESTROY1,0x%p,0x%x\n",
                      details->hwnd, 0 );
            result[ sizeof( result ) - 1 ] = '\0';
            WriteToChannel( result );
        }
        
        break;
        
        
        default:
        break;
    }
    
    return CallNextHookEx( hWndProc, nCode, wParam, lParam );
}

LRESULT CALLBACK CbtProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if ( nCode < 0 ) {
        return CallNextHookEx( hCbtProc, nCode, wParam, lParam );
    }
    
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    char result[ 255 ] = { ""
                         };
    switch ( nCode ) {
        case HCBT_MINMAX:
        
        if ( ( LOWORD( lParam ) == SW_SHOWMINIMIZED )
                || ( LOWORD( lParam ) == SW_MINIMIZE ) ) {
            MessageBox( 0, "Minimizing windows is not allowed in this version. Sorry!", "SeamlessRDP", MB_OK );
            return 1;
        }
        
        GetWindowText( ( HWND ) wParam, windowTitle, 150 );
        
        snprintf( result, sizeof( result ),
                  "SETSTATE1,0x%p,%s,0x%x,0x%x\n",
                  ( HWND ) wParam,
                  windowTitle,
                  LOWORD( lParam ),
                  0 );
        result[ sizeof( result ) - 1 ] = '\0';
        WriteToChannel( result );
        break;
        
        
        default:
        break;
    }
    
    
    
    return CallNextHookEx( hCbtProc, nCode, wParam, lParam );
}


LRESULT CALLBACK ShellProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if ( nCode < 0 ) {
        return CallNextHookEx( hShellProc, nCode, wParam, lParam );
    }
    
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

int WriteToChannel( PCHAR buffer )
{
    PULONG bytesRead = 0;
    PULONG pBytesWritten = 0;
    
    if ( !ChannelIsOpen() )
        return 1;
        
    BOOL result = WTSVirtualChannelWrite( m_vcHandle, buffer, ( ULONG ) strlen( buffer ), pBytesWritten );
    
    if ( result ) {
        return 1;
    } else {
        return 0;
    }
}
