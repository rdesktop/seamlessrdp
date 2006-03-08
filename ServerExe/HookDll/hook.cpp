//
// Copyright (C) 2004-2005 Martin Wickett
//

#include "hook.h"

HINSTANCE WTSWinClipper::mHookDllHinst = 0;
SETHOOKS WTSWinClipper::mSetHooks = 0;
REMOVEHOOKS WTSWinClipper::mRemoveHooks = 0;
GETINSTANCECOUNT WTSWinClipper::mGetInstanceCount = 0;

bool WTSWinClipper::Init()
{
    if ( mHookDllHinst )
        return true;

    while ( true ) {
        // try to load hookdll.dll
        if ( !( mHookDllHinst = LoadLibrary( "hookdll.dll" ) ) )
            break;

        // check number of instances
        if ( !
                ( mGetInstanceCount =
                      ( GETINSTANCECOUNT ) GetProcAddress( mHookDllHinst,
                                                           "GetInstanceCount" ) ) )
            break;


        // get our hook function
        if ( !
                ( mSetHooks =
                      ( SETHOOKS ) GetProcAddress( mHookDllHinst, "SetHooks" ) ) )
            break;

        // get our unkook function
        if ( !
                ( mRemoveHooks =
                      ( REMOVEHOOKS ) GetProcAddress( mHookDllHinst, "RemoveHooks" ) ) )
            break;

        // report success
        return true;
    }

    // if we got here something went wrong
    if ( mHookDllHinst ) {
        FreeLibrary( mHookDllHinst );
        mHookDllHinst = 0;
    }

    return false;
}
