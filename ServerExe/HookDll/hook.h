//*********************************************************************************
//
//Title: Terminal Services Window Clipper
//
//Author: Martin Wickett
//
//Date: 2004
//
//*********************************************************************************

#ifndef __CBT_H__
#define __CBT_H__

#include <windows.h>
#include "hookdll.h"

// 
// a wrapper class to access the WTSWinClipperdll
//
class WTSWinClipper
{
protected:
	static HINSTANCE			mCbtDllHinst;
	static SETHOOKS  			mSetCbtHook;
	static REMOVEHOOKS			mRemoveCbtHook;
	static GETINSTANCECOUNT		mGetInstanceCount;

public:
	static bool Init();
	static void Done()
		{ if (mCbtDllHinst) FreeLibrary(mCbtDllHinst); }

	static void SetCbtHook()
		{ if (mCbtDllHinst) mSetCbtHook();	}

	static void RemoveCbtHook()
		{ if (mCbtDllHinst) mRemoveCbtHook(); }

	static int GetInstanceCount()
	{
		if (mCbtDllHinst)
			return mGetInstanceCount();
		else
			return 0;
	}
};

#endif 