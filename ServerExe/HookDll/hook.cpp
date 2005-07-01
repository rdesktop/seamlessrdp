//
// Copyright (C) 2004-2005 Martin Wickett
//

#include "hook.h"

HINSTANCE WTSWinClipper::mCbtDllHinst = 0;
SETHOOKS WTSWinClipper::mSetCbtHook = 0;
REMOVEHOOKS WTSWinClipper::mRemoveCbtHook = 0;
GETINSTANCECOUNT WTSWinClipper::mGetInstanceCount = 0;

bool WTSWinClipper::Init()
{
    if (mCbtDllHinst)
        return true;

    while (true) {
        // try to load hookdll.dll
        if (!(mCbtDllHinst = LoadLibrary("hookdll.dll")))
            break;

        // check number of instances
        if (!
                (mGetInstanceCount =
                     (GETINSTANCECOUNT) GetProcAddress(mCbtDllHinst,
                                                       "GetInstanceCount")))
            break;


        // get our hook function
        if (!
                (mSetCbtHook =
                     (SETHOOKS) GetProcAddress(mCbtDllHinst, "SetCbtHook")))
            break;

        // get our unkook function
        if (!
                (mRemoveCbtHook =
                     (REMOVEHOOKS) GetProcAddress(mCbtDllHinst, "RemoveCbtHook")))
            break;

        // report success
        return true;
    }

    // if we got here something went wrong
    if (mCbtDllHinst) {
        FreeLibrary(mCbtDllHinst);
        mCbtDllHinst = 0;
    }

    return false;
}
