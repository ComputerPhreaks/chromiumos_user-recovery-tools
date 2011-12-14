// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_CREDITS_DLG_H_
#define CHROMEOSRECOVERY_CREDITS_DLG_H_

#include "resource.h"

class CreditsDlg : public CDialogImpl<CreditsDlg> {
 public:
  enum { IDD = IDD_CREDITS };

  CreditsDlg();

  BEGIN_MSG_MAP(CreditsDlg)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_ID_HANDLER_EX(IDOK, OnOK)
  END_MSG_MAP()

  // WTL calls this function to initialize the dialog.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this function when the user clicks on the OK button.
  void OnOK(__in UINT code, __in int id, __in HWND ctrl);

 private:
  static const WCHAR kCredits[];

  DISALLOW_COPY_AND_ASSIGN(CreditsDlg);
};

#endif  // CHROMEOSRECOVERY_CREDITS_DLG_H_
