//
// Copyright (C) 2004-2005 Martin Wickett
//

#ifndef __HOOK_H__
#define __HOOK_H__

#include <windows.h>
#include "hookdll.h"

//
// a wrapper class to access the WTSWinClipperdll
//
class WTSWinClipper
{
    protected:
        static HINSTANCE mHookDllHinst;
        static SETHOOKS mSetHooks;
        static REMOVEHOOKS mRemoveHooks;
        static GETINSTANCECOUNT mGetInstanceCount;

    public:
        static bool Init();
        static void Done()
        {
            if ( mHookDllHinst )
                FreeLibrary( mHookDllHinst );
        }

        static void SetHooks()
        {
            if ( mHookDllHinst )
                mSetHooks();
        }

        static void RemoveHooks()
        {
            if ( mHookDllHinst )
                mRemoveHooks();
        }

        static int GetInstanceCount()
        {
            if ( mHookDllHinst )
                return mGetInstanceCount();
            else
                return 0;
        }
};

#endif
