// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "credits_dlg.h"

// static
const WCHAR CreditsDlg::kCredits[] =
    L"zlib\r\n\r\n" \
    L"// THIS FILE is almost entirely based upon code by Jean-loup Gailly\r\n" \
    L"// and Mark Adler. It has been modified by Lucian Wischik.\r\n" \
    L"// The modifications were: incorporate the bugfixes of 1.1.4, allow\r\n" \
    L"// unzipping to/from handles/pipes/files/memory, encryption, unicode,\r\n" \
    L"// a windowsish api, and putting everything into a single .cpp file.\r\n" \
    L"// The original code may be found at http://www.gzip.org/zlib/\r\n" \
    L"// The original copyright text follows.\r\n" \
    L"//\r\n" \
    L"//\r\n" \
    L"//\r\n" \
    L"// zlib.h -- interface of the 'zlib' general purpose compression library\r\n" \
    L"//  version 1.1.3, July 9th, 1998\r\n" \
    L"//\r\n" \
    L"//  Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler\r\n" \
    L"//\r\n" \
    L"//  This software is provided 'as-is', without any express or implied\r\n" \
    L"//  warranty.  In no event will the authors be held liable for any damages\r\n" \
    L"//  arising from the use of this software.\r\n" \
    L"//\r\n" \
    L"//  Permission is granted to anyone to use this software for any purpose,\r\n" \
    L"//  including commercial applications, and to alter it and redistribute it\r\n" \
    L"//  freely, subject to the following restrictions:\r\n" \
    L"//\r\n" \
    L"//  1. The origin of this software must not be misrepresented; you must not\r\n" \
    L"//     claim that you wrote the original software. If you use this software\r\n" \
    L"//     in a product, an acknowledgment in the product documentation would be\r\n" \
    L"//     appreciated but is not required.\r\n" \
    L"//  2. Altered source versions must be plainly marked as such, and must not be\r\n" \
    L"//     misrepresented as being the original software.\r\n" \
    L"//  3. This notice may not be removed or altered from any source distribution.\r\n" \
    L"//\r\n" \
    L"//  Jean-loup Gailly        Mark Adler\r\n" \
    L"//  jloup@gzip.org          madler@alumni.caltech.edu\r\n";

CreditsDlg::CreditsDlg() {
}

// Message handlers

BOOL CreditsDlg::OnInitDialog(__in HWND /*focus*/, __in LPARAM /*param*/) {
  CenterWindow();
  CFontHandle font = GetFont();
  CLogFont log_font;
  font.GetLogFont(&log_font);
  StringCchCopy(log_font.lfFaceName, LF_FACESIZE, L"Courier New");
  log_font.lfHeight = 14;
  font = log_font.CreateFontIndirect();
  CEdit credits_ctrl;
  credits_ctrl.Attach(GetDlgItem(IDC_CREDITS_TEXT));
  credits_ctrl.SetWindowText(kCredits);
  credits_ctrl.SetFont(font);
  return TRUE;
}

void CreditsDlg::OnOK(__in UINT /*code*/, __in int /*id*/, __in HWND /*ctrl*/) {
  EndDialog(IDOK);
}
