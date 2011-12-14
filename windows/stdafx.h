// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CHROMEOSRECOVERY_STDAFX_H_
#define  CHROMEOSRECOVERY_STDAFX_H_

#define WINVER                                    0x0501
#define _WIN32_WINNT                              0x0501
#define _WIN32_IE                                 0x0501
#define _RICHEDIT_VER                             0x0200
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES   1
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS
#define _ATL_USE_CSTRING_FLOAT
#define _ATL_NO_EXCEPTIONS
#define _SECURE_ATL                               1

#ifdef OS_WINDOWS
#undef OS_WINDOWS
#endif

#include <atlbase.h>
#include <atlapp.h>

#include <wchar.h>
#include <strsafe.h>
#include <atlwin.h>
#include <atldlgs.h>
#include <atlmisc.h>
#include <atlddx.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlfile.h>
#include <winhttp.h>
#include <windns.h>
#include <dbt.h>
#include <wbemcli.h>
#include <comutil.h>

#pragma warning(disable: 4995)
#include <string>
#include <xstring>
#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CROS_PSWIZB_LOCAL_IMAGE   0x00001000
#define CROS_PSN_LOCAL_IMAGE      (PSN_LAST + 19)

#endif   // CHROMEOSRECOVERY_STDAFX_H_
