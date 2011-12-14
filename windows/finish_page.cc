// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "finish_page.h"

FinishPage::FinishPage(__in ChromeOSRecoveryWizard* parent)
    : parent_(parent) {
}

FinishPage::~FinishPage() {
}

// Message handlers

BOOL FinishPage::OnInitDialog(__in HWND focus, __in LPARAM param) {
  DoDataExchange();
  return TRUE;
}

// Notifications

int FinishPage::OnSetActive() {
  parent_->SetWizardButtons(PSWIZB_FINISH);
  ::ShowWindow(parent_->GetDlgItem(ID_WIZBACK), SW_HIDE);
  ::ShowWindow(parent_->GetDlgItem(IDCANCEL), SW_HIDE);
  return 0;
}
