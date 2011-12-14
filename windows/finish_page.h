// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_FINISHPAGE_H_
#define CHROMEOSRECOVERY_FINISHPAGE_H_

#include "resource.h"

class ChromeOSRecoveryWizard;

// Final page of the wizard.
class FinishPage : public CPropertyPageImpl<FinishPage>,
                   public CWinDataExchange<FinishPage> {
 public:
  enum { IDD = IDD_PROPPAGE_FINISH };

  explicit FinishPage(__in ChromeOSRecoveryWizard* parent);
  virtual ~FinishPage();

  BEGIN_MSG_MAP(FinishPage)
    MSG_WM_INITDIALOG(OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<FinishPage>)
  END_MSG_MAP()

  BEGIN_DDX_MAP(FinishPage)
    DDX_CONTROL(IDC_HOW_TO_USE_LINK, how_to_use_link_);
  END_DDX_MAP()

  // WTL calls this method to initialize the page.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this method when the page is set active.
  int OnSetActive();

 private:
  // The wizard object.
  ChromeOSRecoveryWizard* parent_;

  // Hyperlink control that points to a link that tells the user how to use
  // the recovery media.
  CHyperLink how_to_use_link_;

  DISALLOW_COPY_AND_ASSIGN(FinishPage);
};

#endif  // CHROMEOSRECOVERY_FINISHPAGE_H_
