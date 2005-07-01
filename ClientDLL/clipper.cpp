//
// Copyright (C) 2004-2005 Martin Wickett
//

#define TSDLL

#include "clipper.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);
    UNREFERENCED_PARAMETER(hinstDLL);

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        break;

    default:
        break;
    }
    return TRUE;
}

void WINAPI VirtualChannelOpenEvent(DWORD openHandle, UINT event,
                                    LPVOID pdata, UINT32 dataLength,
                                    UINT32 totalLength, UINT32 dataFlags)
{
    LPDWORD pdwControlCode = (LPDWORD) pdata;
    CHAR ourData[1600];
    UINT ui = 0;

    UNREFERENCED_PARAMETER(openHandle);
    UNREFERENCED_PARAMETER(dataFlags);

    ZeroMemory(ourData, sizeof(ourData));

    //copy the send string (with the same lenth of the data)
    strncpy(ourData, (LPSTR) pdata, dataLength / sizeof(char));

    if (OUTPUT_DEBUG_INFO == 1) {
        OutputDebugString
        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Virtual channel data received");
        OutputDebugString(ourData);
    }

    if (dataLength == totalLength) {
        switch (event) {
        case CHANNEL_EVENT_DATA_RECEIVED: {
                CTokenizer tok(_T((LPSTR) ourData), _T(";"));
                CStdString cs;

                CWindowData *wid = new CWindowData("");
                CStdString messageType;
                int mixMaxType = 0;

                while (tok.Next(cs)) {
                    CStdString msg;
                    CTokenizer msgTok(cs, _T("="));

                    msgTok.Next(msg);

                    if (strcmp(msg, "MSG") == 0) {
                        msgTok.Next(msg);
                        messageType = msg;
                    }

                    if (strcmp(msg, "ID") == 0) {
                        msgTok.Next(msg);
                        wid->SetId(msg);
                    } else if (strcmp(msg, "TITLE") == 0) {
                        msgTok.Next(msg);
                        wid->SetTitle(msg);
                    } else if (strcmp(msg, "POS") == 0) {
                        msgTok.Next(msg);

                        CStdString pos;
                        CTokenizer posTok(msg, _T("~"));

                        posTok.Next(pos);


                        // check bounds, coords can be negative if window top left point is moved off the screen.
                        // we don't care about that since the window can't be see so just use zero.

                        if (strchr(pos, '-') == NULL) {
                            wid->SetX1(atoi(pos));
                        } else {
                            wid->SetX1(0);
                        }

                        posTok.Next(pos);

                        if (strchr(pos, '-') == NULL) {
                            wid->SetY1(atoi(pos));
                        } else {
                            wid->SetY1(0);
                        }

                        posTok.Next(pos);

                        if (strchr(pos, '-') == NULL) {
                            wid->SetX2(atoi(pos));
                        } else {
                            wid->SetX2(0);
                        }

                        posTok.Next(pos);

                        if (strchr(pos, '-') == NULL) {
                            wid->SetY2(atoi(pos));
                        } else {
                            wid->SetY2(0);
                        }
                    } else if (strcmp(msg, "TYPE") == 0) {
                        msgTok.Next(msg);
                        mixMaxType = atoi(msg);
                    }
                }

                if (strcmp(messageType, "HSHELL_WINDOWCREATED") == 0) {
                    if (OUTPUT_DEBUG_INFO == 1) {
                        OutputDebugString
                        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Message was of type HSHELL_WINDOWCREATED window title is:");
                        OutputDebugString(wid->GetTitle());
                    }

                    CStdString s = wid->GetId();
                    char *ptr;
                    int length = s.GetLength();
                    ptr = s.GetBufferSetLength(length);

                    hash_insert(ptr, wid, &m_ht);

                    CreateAndShowWindow(wid);

                    DoClipping(1);
                } else if (strcmp(messageType, "HSHELL_WINDOWDESTROYED") == 0) {
                    if (OUTPUT_DEBUG_INFO == 1) {
                        OutputDebugString
                        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Message was of type HSHELL_WINDOWDISTROYED window title is:");
                        OutputDebugString(wid->GetTitle());
                    }

                    CStdString s = wid->GetId();
                    char *ptr;
                    int length = s.GetLength();
                    ptr = s.GetBufferSetLength(length);

                    CWindowData *oldWinData =
                        (CWindowData *) hash_del(ptr, &m_ht);

                    DestroyTaskbarWindow(oldWinData);

                    delete oldWinData;

                    DoClipping(1);
                } else if (strcmp(messageType, "HCBT_MINMAX") == 0) {
                    if (OUTPUT_DEBUG_INFO == 1) {
                        OutputDebugString
                        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Message was of type HCBT_MINMAX");
                    }


                    //TODO

                } else if (strcmp(messageType, "HCBT_MOVESIZE") == 0) {
                    if (OUTPUT_DEBUG_INFO == 1) {
                        OutputDebugString
                        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Message was of type HCBT_MOVESIZE window title is:");
                        OutputDebugString(wid->GetTitle());
                    }

                    CStdString s = wid->GetId();
                    char *ptr;
                    int length = s.GetLength();
                    ptr = s.GetBufferSetLength(length);

                    CWindowData *movedWinData =
                        (CWindowData *) hash_lookup(ptr, &m_ht);

                    if (movedWinData != NULL) {
                        movedWinData->SetX1(wid->GetX1());
                        movedWinData->SetX2(wid->GetX2());
                        movedWinData->SetY1(wid->GetY1());
                        movedWinData->SetY2(wid->GetY2());

                        DoClipping(1);
                    }

                    delete wid;
                } else if (strcmp(messageType, "CALLWNDPROC_WM_MOVING") == 0) {
                    if (OUTPUT_DEBUG_INFO == 1) {
                        OutputDebugString
                        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Message was of type CALLWNDPROC_WM_MOVING window title is:");
                        OutputDebugString(wid->GetTitle());
                    }

                    CStdString s = wid->GetId();
                    char *ptr;
                    int length = s.GetLength();
                    ptr = s.GetBufferSetLength(length);

                    CWindowData *movedWinData =
                        (CWindowData *) hash_lookup(ptr, &m_ht);

                    if (movedWinData != NULL) {
                        movedWinData->SetX1(wid->GetX1());
                        movedWinData->SetX2(wid->GetX2());
                        movedWinData->SetY1(wid->GetY1());
                        movedWinData->SetY2(wid->GetY2());

                        ////might be too much of an overhead forcing the redraw here. Might be better to do 'DoClipping(0)' instead?
                        DoClipping(1);
                    }

                    delete wid;
                }
            }
            break;

        case CHANNEL_EVENT_WRITE_COMPLETE: {}
            break;

        case CHANNEL_EVENT_WRITE_CANCELLED: {}
            break;

        default: {}
            break;
        }
    } else {}
}


VOID VCAPITYPE VirtualChannelInitEventProc(LPVOID pInitHandle, UINT event,
        LPVOID pData, UINT dataLength)
{
    UINT ui;

    UNREFERENCED_PARAMETER(pInitHandle);
    UNREFERENCED_PARAMETER(dataLength);

    switch (event) {
    case CHANNEL_EVENT_INITIALIZED: {}
        break;

    case CHANNEL_EVENT_CONNECTED: {
            //
            // open channel
            //
            ui = gpEntryPoints->pVirtualChannelOpen(gphChannel,
                                                    &gdwOpenChannel,
                                                    CHANNELNAME,
                                                    (PCHANNEL_OPEN_EVENT_FN)
                                                    VirtualChannelOpenEvent);

            if (ui == CHANNEL_RC_OK) {}
            else {
                MessageBox(NULL, TEXT("Open of RDP virtual channel failed"),
                           TEXT("TS Window Clipper"), MB_OK);
            }

            if (ui != CHANNEL_RC_OK) {
                return;
            }
        }
        break;

    case CHANNEL_EVENT_V1_CONNECTED: {
            MessageBox(NULL,
                       TEXT
                       ("Connecting to a non Windows 2000 Terminal Server"),
                       TEXT("TS Window Clipper"), MB_OK);
        }
        break;

    case CHANNEL_EVENT_DISCONNECTED: {}
        break;

    case CHANNEL_EVENT_TERMINATED: {
            //
            // free the entry points table
            //
            LocalFree((HLOCAL) gpEntryPoints);
        }
        break;

    default: {}
        break;
    }
}

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
    CHANNEL_DEF cd;
    UINT uRet;

    size_t s = 10;
    hash_construct_table(&m_ht, s);

    //
    // allocate memory
    //
    gpEntryPoints =
        (PCHANNEL_ENTRY_POINTS) LocalAlloc(LPTR, pEntryPoints->cbSize);

    memcpy(gpEntryPoints, pEntryPoints, pEntryPoints->cbSize);

    //
    // initialize CHANNEL_DEF structure
    //
    ZeroMemory(&cd, sizeof(CHANNEL_DEF));
    strcpy(cd.name, CHANNELNAME);       // ANSI ONLY

    //
    // register channel
    //
    uRet =
        gpEntryPoints->pVirtualChannelInit((LPVOID *) & gphChannel,
                                           (PCHANNEL_DEF) & cd, 1,
                                           VIRTUAL_CHANNEL_VERSION_WIN2000,
                                           (PCHANNEL_INIT_EVENT_FN)
                                           VirtualChannelInitEventProc);

    if (uRet == CHANNEL_RC_OK) {
        if (ALWAYS__CLIP) {
            DoClipping(1);
        }
    } else {
        MessageBox(NULL, TEXT("RDP Virtual channel Init Failed"),
                   TEXT("TS Window Clipper"), MB_OK);
    }

    if (uRet != CHANNEL_RC_OK) {
        return FALSE;
    }

    //
    // make sure channel was initialized
    //
    if (cd.options != CHANNEL_OPTION_INITIALIZED) {
        return FALSE;
    }

    return TRUE;
}


// data structure to transfer informations
typedef struct _WindowFromProcessOrThreadID
{
    union
    {
        DWORD procId;
        DWORD threadId;
    };
    HWND hWnd;
}
Wnd4PTID;

// Callback procedure
BOOL CALLBACK PrivateEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    DWORD procId;
    DWORD threadId;
    Wnd4PTID *tmp = (Wnd4PTID *) lParam;
    // get the process/thread id of current window
    threadId = GetWindowThreadProcessId(hwnd, &procId);
    // check if the process/thread id equal to the one passed by lParam?
    if (threadId == tmp->threadId || procId == tmp->procId) {
        // check if the window is a main window
        // because there lots of windows belong to the same process/thread
        LONG dwStyle = GetWindowLong(hwnd, GWL_STYLE);
        if (dwStyle & WS_SYSMENU) {
            tmp->hWnd = hwnd;
            return FALSE;       // break the enumeration
        }
    }
    return TRUE;                // continue the enumeration
}

// Enumarate all the MainWindow of the system
HWND FindProcessMainWindow(DWORD procId)
{
    Wnd4PTID tempWnd4ID;
    tempWnd4ID.procId = procId;
    if (!EnumWindows
            ((WNDENUMPROC) PrivateEnumWindowsProc, (LPARAM) & tempWnd4ID)) {

        if (OUTPUT_DEBUG_INFO == 1) {
            OutputDebugString
            ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Found main process window");
        }

        return tempWnd4ID.hWnd;
    }


    if (OUTPUT_DEBUG_INFO == 1) {
        OutputDebugString
        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Could not find main process window");
    }

    return NULL;
}


void DoClipping(int forceRedraw)
{
    //if main window handle is null, try to get it
    if (m_mainWindowHandle == NULL) {
        m_mainWindowHandle = FindProcessMainWindow(GetCurrentProcessId());

        //hide the window from taskbar and put at the back of the z order
        if (HIDE_TSAC_WINDOW == 1) {
            ShowWindow(m_mainWindowHandle, SW_HIDE);
            SetWindowLongPtr(m_mainWindowHandle, GWL_EXSTYLE,
                             GetWindowLong(m_mainWindowHandle,
                                           GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
            ShowWindow(m_mainWindowHandle, SW_SHOW);
        }

        SetWindowPos(m_mainWindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    }

    //if we have the handle, lets use it for the clipping
    if (m_mainWindowHandle != NULL) {
        RECT wRect;
        GetWindowRect(m_mainWindowHandle, &wRect);

        if (OUTPUT_DEBUG_INFO == 1) {
            OutputDebugString
            ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Restarting clipping...");
        }

        m_regionResult = NULL;

        if (OUTPUT_WINDOW_TABLE_DEBUG_INFO == 1) {
            OutputDebugString
            ("-----------------------------------------------------------------------------");
            OutputDebugString
            ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> starting printing of window table");
        }

        //enumerate though hashtable
        if (&m_ht != NULL) {
            hash_enumerate(&m_ht, CreateRegionFromWindowData);
        }

        if (OUTPUT_WINDOW_TABLE_DEBUG_INFO == 1) {
            OutputDebugString
            ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> finished printing of window table");
            OutputDebugString
            ("-----------------------------------------------------------------------------");
        }

        if (m_regionResult == NULL) {
            if (ALWAYS__CLIP) {
                m_regionResult = CreateRectRgn(0, 0, 0, 0);
            } else {
                m_regionResult =
                    CreateRectRgn(0, 0, wRect.right, wRect.bottom);
            }
        }

        SetWindowRgn(m_mainWindowHandle, (HRGN__ *) m_regionResult, TRUE);

        if (forceRedraw == 1) {
            // invalidate the window and force it to redraw
            RedrawWindow(m_mainWindowHandle, NULL, NULL,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
    } else {
        if (OUTPUT_DEBUG_INFO == 1) {
            OutputDebugString
            ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Coulf not find window to clip");
        }
    }
}

void CreateRegionFromWindowData(char *key, void *value)
{
    CWindowData *wd;
    wd = (CWindowData *) value;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    char strB[5];
    char strT[5];
    char strL[5];
    char strR[5];

    if (m_regionResult == NULL) {
        m_regionResult = CreateRectRgn(0, 0, 0, 0);
    }

    if (OUTPUT_DEBUG_INFO == 1 && OUTPUT_WINDOW_TABLE_DEBUG_INFO != 1) {
        OutputDebugString
        ("TS WINDOW CLIPPER :: CLIENT DLL :: Info --> Adding this window to cliping region");
        OutputDebugString(wd->GetTitle());
    }
    if (OUTPUT_WINDOW_TABLE_DEBUG_INFO == 1) {
        ltoa(wd->GetY2(), strB, 10);
        ltoa(wd->GetY1(), strT, 10);
        ltoa(wd->GetX2(), strR, 10);
        ltoa(wd->GetX1(), strL, 10);

        OutputDebugString("This window is in the table:");
        OutputDebugString(wd->GetTitle());
        OutputDebugString(wd->GetId());
        OutputDebugString(strL);
        OutputDebugString(strT);
        OutputDebugString(strR);
        OutputDebugString(strB);
        OutputDebugString("*******************");
    }

    HRGN newRegion =
        CreateRectRgn(wd->GetX1(), wd->GetY1(), wd->GetX2(), wd->GetY2());

    CombineRgn(m_regionResult, newRegion, m_regionResult, RGN_OR);
}

/*
   Dummy procedure to catch when window is being maximised.
 
   Need to tell the window on the server to do the same.
 */
LRESULT CALLBACK DummyWindowCallbackProc(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam)
{
    //TODO

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateAndShowWindow(CWindowData * wd)
{
    if (classAlreadyRegistered == 0) {
        static const char *szWndName = "WTSWinClipperDummy";
        WNDCLASS wc;

        wc.style = 0;
        wc.lpfnWndProc = DummyWindowCallbackProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = 0;
        wc.hIcon = 0;
        wc.hCursor = 0;
        wc.hbrBackground = 0;
        wc.lpszMenuName = 0;
        wc.lpszClassName = szWndName;

        if (RegisterClass(&wc)) {
            classAlreadyRegistered = 1;
        }
    }

    if (classAlreadyRegistered = 1) {
        HWND hWnd =
            CreateWindow(TEXT("WTSWinClipperDummy"), wd->GetTitle(), WS_POPUP,
                         0, 0, 0, 0, 0, 0, 0, 0);
        ShowWindow(hWnd, 3);
        SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOREDRAW);
        wd->TaskbarWindowHandle = hWnd;
        SetFocus(m_mainWindowHandle);
    }
}

void DestroyTaskbarWindow(CWindowData * wd)
{
    if (wd->TaskbarWindowHandle != NULL) {
        DestroyWindow(wd->TaskbarWindowHandle);
    }
}
