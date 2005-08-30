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

bool bHooked = false;
bool bHooked2 = false;
bool bHooked3 = false;
HHOOK hhook = 0; //cbt
HHOOK hhook2 = 0; //shell
HHOOK hhook3 = 0; //wnd proc
HINSTANCE hInst = 0;
HANDLE m_vcHandle = 0;


void SendDebug( char *format, ... )
{
    va_list argp;
    char buf [ 256 ];
    
    va_start( argp, format );
    vsprintf( buf, format, argp );
    va_end( argp );
    
    if ( ChannelIsOpen() ) {
        WriteToChannel( "DEBUG1," );
        WriteToChannel( buf );
        WriteToChannel( "\n" );
    }
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
        return CallNextHookEx( hhook3, nCode, wParam, lParam );
    }
    
    PCHAR buffer = NULL;
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    HWND windowHandle2 = NULL;
    char result[ 255 ] = { ""
                         };
    char strWindowId[ 25 ];
    char type[ 25 ];
    
    LONG b, t, l, r;
    char strX[ 5 ];
    char strY[ 5 ];
    char strW[ 5 ];
    char strH[ 5 ];
    
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
                  "POSITION1,0x%x,%d,%d,%d,%d,0x%x",
                  ( int ) details->hwnd,
                  rect->left, rect->top,
                  rect->right - rect->left,
                  rect->bottom - rect->top,
                  0 );
        result[ sizeof( result ) - 1 ] = '\0';
        buffer = result;
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
                      "ZCHANGE1,0x%x,0x%x,0x%x\n",
                      details->hwnd,
                      wp->flags & SWP_NOACTIVATE ? wp->hwndInsertAfter : 0,
                      0 );
            result[ sizeof( result ) - 1 ] = '\0';
            buffer = result;
        }
        break;
        
        
        case WM_CREATE:
        if ( cs->style & WS_DLGFRAME ) {
            snprintf( result, sizeof( result ),
                      "CREATE1,0x%x,0x%x\n",
                      details->hwnd, 0 );
            buffer = result;
        }
        break;
        
        
        case WM_DESTROY:
        if ( dwStyle & WS_DLGFRAME ) {
            snprintf( result, sizeof( result ),
                      "DESTROY1,0x%x,0x%x\n",
                      details->hwnd, 0 );
            buffer = result;
        }
        
        break;
        
        
        default:
        break;
    }
    
    if ( ChannelIsOpen() ) {
        if ( buffer != NULL ) {
            WriteToChannel( buffer );
        }
    }
    
    return CallNextHookEx( hhook3, nCode, wParam, lParam );
}

LRESULT CALLBACK CbtProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if ( nCode < 0 ) {
        return CallNextHookEx( hhook, nCode, wParam, lParam );
    }
    
    
    PCHAR buffer = NULL;
    
    
    char windowTitle[ 150 ] = { ""
                              };
    HWND windowHandle = NULL;
    char result[ 255 ] = { ""
                         };
    char strWindowId[ 25 ];
    char type[ 25 ];
    
    
    LONG b, t, l, r;
    char strW[ 5 ];
    char strY[ 5 ];
    char strX[ 5 ];
    char strH[ 5 ];
    RECT rect;
    
    int i = 0; //tmp
    
    switch ( nCode ) {
        case HCBT_MINMAX:
        
        windowHandle = ( HWND ) wParam;
        //get win name
        GetWindowText( windowHandle, windowTitle, 150 );
        
        //get an id for it
        itoa( ( int ) windowHandle, strWindowId, 10 );
        
        //get operation type(min,max). if max, do not clip at all,if min use window's previous coords
        //codes are:
        
        // SW_HIDE= 0  SW_SHOWNORMAL=1  SW_NORMAL=1  SW_SHOWMINIMIZED=2  SW_SHOWMAXIMIZED=3  SW_MAXIMIZE=3
        // SW_SHOWNOACTIVATE=4  SW_SHOW=5  SW_MINIMIZE=6  SW_SHOWMINNOACTIVE=7  SW_SHOWNA=8  SW_RESTORE=9
        // SW_SHOWDEFAULT=10  SW_FORCEMINIMIZE=11  SW_MAX=11
        
        itoa( ( int ) lParam, type, 10 );
        
        //get coords
        GetWindowRect( windowHandle, &rect );
        b = rect.bottom;
        t = rect.top;
        l = rect.left;
        r = rect.right;
        ltoa( b - t, strW, 10 );
        ltoa( t, strY, 10 );
        ltoa( r - l, strH, 10 );
        ltoa( l, strX, 10 );
        
        //get name
        GetWindowText( windowHandle, windowTitle, 150 );
        
        ////setup return string
        strcat( result, "MSG=HCBT_MINMAX;OP=4;" );
        
        // SW_SHOWNOACTIVATE=4  SW_SHOW=5  SW_MINIMIZE=6  SW_SHOWMINNOACTIVE=7  SW_SHOWNA=8  SW_RESTORE=9
        // SW_SHOWDEFAULT=10  SW_FORCEMINIMIZE=11  SW_MAX=11
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
        strcat( result, ";" );
        strcat( result, "TYPE=" );
        strcat( result, type );
        strcat( result, "." );
        
        //-------------------------------------------------------------------------------------------------
        // code to prevent minimising windows (can be removed once minimise has been implemented)
        //i = (int)lParam;
        //if (i==0 || i==2 || i==6 || i==7 || i==8 || i==11)
        //if ( i==2 || i==6 )
        //{
        // MessageBox(0,"Minimising windows is not allowed in this version. Sorry!","TS Window Clipper", MB_OK);
        // return 1;
        //}
        //-----------------------------------------------------------------------------------------
        
        //-------------------------------------------------------------------------------------------------
        // code to prevent maximising windows (can be removed once maximise has been implemented)
        //i = (int)lParam;
        //if (i==3 || i==9 || i==11)
        //if (i==3 || i==11)
        //{
        // MessageBox(0,"Maximising windows is not allowed in this version. Sorry!","TS Window Clipper", MB_OK);
        // return 1;
        //}
        //-----------------------------------------------------------------------------------------
        
        buffer = result;
        
        break;
        
        default:
        break;
    }
    
    if ( ChannelIsOpen() ) {
        if ( buffer != NULL ) {
            WriteToChannel( buffer );
        }
    }
    
    return CallNextHookEx( hhook, nCode, wParam, lParam );
}


LRESULT CALLBACK ShellProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if ( nCode < 0 ) {
        return CallNextHookEx( hhook, nCode, wParam, lParam );
    }
    
    if ( ChannelIsOpen() ) {
        PCHAR buffer = NULL;
        
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
            
            buffer = result;
            
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
            
            buffer = result;
            
            break;
            default:
            break;
        }
        
        if ( buffer != NULL ) {
            WriteToChannel( buffer );
        }
    }
    
    return CallNextHookEx( hhook, nCode, wParam, lParam );
}

DLL_EXPORT void SetCbtHook( void )
{
    if ( !bHooked ) {
        hhook = SetWindowsHookEx( WH_CBT, ( HOOKPROC ) CbtProc, hInst, ( DWORD ) NULL );
        bHooked = true;
    }
    
    if ( !bHooked2 ) {
        hhook2 = SetWindowsHookEx( WH_SHELL, ( HOOKPROC ) ShellProc, hInst, ( DWORD ) NULL );
        bHooked2 = true;
    }
    
    if ( !bHooked3 ) {
        hhook3 = SetWindowsHookEx( WH_CALLWNDPROC, ( HOOKPROC ) CallWndProc, hInst, ( DWORD ) NULL );
        bHooked3 = true;
    }
}

DLL_EXPORT void RemoveCbtHook( void )
{
    if ( bHooked ) {
        UnhookWindowsHookEx( hhook );
        bHooked = false;
    }
    
    if ( bHooked2 ) {
        UnhookWindowsHookEx( hhook2 );
        bHooked2 = false;
    }
    
    if ( bHooked3 ) {
        UnhookWindowsHookEx( hhook3 );
        bHooked3 = false;
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
    
    BOOL result = WTSVirtualChannelWrite( m_vcHandle, buffer, ( ULONG ) strlen( buffer ), pBytesWritten );
    
    if ( result ) {
        return 1;
    } else {
        return 0;
    }
}
