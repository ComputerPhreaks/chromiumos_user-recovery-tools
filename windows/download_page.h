// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_DOWNLOADPAGE_H_
#define CHROMEOSRECOVERY_DOWNLOADPAGE_H_

#include "resource.h"
#include "progress_dlg.h"

class ChromeOSRecoveryWizard;
class Device;
template<class T> class Operation;

// Wizard page that downloads the recovery image (zip file), verifies the
// content against the SHA1/MD5 hash in the configuration data, and then
// unpacks the content to a temporary file which is used by the write image
// page.
class DownloadPage : public CPropertyPageImpl<DownloadPage>,
                     public ProgressDlg<DownloadPage> {
 public:
  enum { IDD = IDD_PROPPAGE_PROGRESS };

  explicit DownloadPage(__in ChromeOSRecoveryWizard* parent);
  virtual ~DownloadPage();

  BEGIN_MSG_MAP(DownloadPage)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancelInternal)
    MESSAGE_HANDLER_EX(WM_CROS_DOWNLOAD_DONE, OnDownloadDone)
    MESSAGE_HANDLER_EX(WM_CROS_VERIFY_DONE, OnVerifyDone)
    MESSAGE_HANDLER_EX(WM_CROS_UNZIP_DONE, OnUnzipDone)
    CHAIN_MSG_MAP(CPropertyPageImpl<DownloadPage>)
  END_MSG_MAP()

  // WTL calls this method to initialize the page.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this method when the download operation completes.
  LRESULT OnDownloadDone(__in UINT msg,
                         __in WPARAM wparam,
                         __in LPARAM lparam);

  // WTL calls this method when the verification operation completes.
  LRESULT OnVerifyDone(__in UINT msg, __in WPARAM wparam, __in LPARAM lparam);

  // WTL calls this method when the unzip operation completes.
  LRESULT OnUnzipDone(__in UINT msg, __in WPARAM wparam, __in LPARAM lparam);

  // WTL calls this method when the page is set active.
  int OnSetActive();

  // WTL calls this method when the user clicks on the Cancel button in the
  // wizard to give the page a chance to veto the cancel.
  BOOL OnQueryCancel();

  CString downloaded_file();

 private:
  // Resets the state of this page.
  void Reset();

  // The base class calls this method when an operation has been cancelled.
  void OnOperationCancelled();

  // The wizard object.
  ChromeOSRecoveryWizard* parent_;

  // The selected device.
  Device* device_;

  // The current operation (download, verify or unzip).
  Operation<DownloadPage>* curr_operation_;

  // The name of the downloaded file.
  CString downloaded_file_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPage);
};

#endif  // CHROMEOSRECOVERY_DOWNLOADPAGE_H_
