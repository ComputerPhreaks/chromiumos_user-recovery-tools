// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_CHROMEOSRECOVERYWIZARD_H_
#define CHROMEOSRECOVERY_CHROMEOSRECOVERYWIZARD_H_

#include "resource.h"
#include "select_device_page.h"
#include "select_usb_page.h"
#include "download_page.h"
#include "write_image_page.h"
#include "finish_page.h"

// The Windows Recovery Tool wizard, the main UI.
class ChromeOSRecoveryWizard
    : public CPropertySheetImpl<ChromeOSRecoveryWizard> {
 public:
  explicit ChromeOSRecoveryWizard(__in HWND parent = NULL);

  BEGIN_MSG_MAP(ChromeOSRecoveryWizard)
    MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnWindowPosChanged)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    COMMAND_ID_HANDLER(IDC_LOCAL_IMAGE, OnLocalImage)
    CHAIN_MSG_MAP(CPropertySheetImpl<ChromeOSRecoveryWizard>)
  END_MSG_MAP()

  // Determines which buttons are shown in the wizard.
  void SetWizardButtons(DWORD flags);

  // WTL calls this method when the wizard has been initialized.
  void OnSheetInitialized();

  // WTL calls this method when the wizard has been moved.
  LRESULT OnWindowPosChanged(UINT msg,
                             WPARAM wparam,
                             LPARAM lparam,
                             BOOL& handled);

  // WTL calls this method when the user clicks on the system menu.
  void OnSysCommand(__in UINT id, __in CPoint point);

  // WTL calls this method when the user clicks on the Local Image button.
  LRESULT OnLocalImage(__in WORD code,
                       __in WORD id,
                       __in HWND ctrl,
                       __inout BOOL& handled);

  SelectDevicePage* select_device_page();
  SelectUSBPage* select_usb_page();
  DownloadPage* download_page();
  WriteImagePage* write_image_page();
  FinishPage* finish_page();
  BOOL restart_wizard();
  void set_restart_wizard(BOOL restart);

  static const int kSelectDevicePageIndex;
  static const int kSelectUSBPageIndex;
  static const int kDownloadPageIndex;
  static const int kWriteImagePageIndex;
  static const int kFinishPageIndex;

 private:
  // Page that allows a user to select a Chrome OS device they want to recover.
  SelectDevicePage select_device_page_;

  // Page that allows user to select the recovery media.
  SelectUSBPage select_usb_page_;

  // Page that downloads, validates and unpacks recovery image.
  DownloadPage download_page_;

  // Page that writes recovery image to removable media.
  WriteImagePage write_image_page_;

  // Final page that tells the user the recovery media is ready.
  FinishPage finish_page_;

  // Local image button.
  CButton local_image_button_;

  // Flag to tell the main thread to restart the wizard.
  // This flag is set when the user changes the UI language.
  BOOL restart_wizard_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSRecoveryWizard);
};

#endif  // CHROMEOSRECOVERY_CHROMEOSRECOVERYWIZARD_H_
