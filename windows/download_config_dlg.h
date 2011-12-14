// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_DOWNLOADCONFIGDLG_H_
#define CHROMEOSRECOVERY_DOWNLOADCONFIGDLG_H_

#include "progress_dlg.h"

template<class T> class HttpDownloadOperation;

// Dialog that downloads the Chrome OS recovery information from Google.
class DownloadConfigDlg : public CDialogImpl<DownloadConfigDlg>,
                          public ProgressDlg<DownloadConfigDlg> {
 public:
  enum {IDD = IDD_DOWNLOAD_CONFIG};

  explicit DownloadConfigDlg(__in const CString& url);
  virtual ~DownloadConfigDlg();

  BEGIN_MSG_MAP(DownloadConfigDlg)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancelInternal)
    MESSAGE_HANDLER_EX(WM_CROS_DOWNLOAD_DONE, OnDownloadDone)
  END_MSG_MAP()

  // WTL calls this function to initialize the dialog.
  virtual BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this function when the user clicks on the Cancel button.
  virtual void OnCancel(__in UINT code, __in int id, __in HWND ctrl);

  // WTL calls this function when the download operation completes.
  LRESULT OnDownloadDone(__in UINT msg,
                         __in WPARAM wparam,
                         __in LPARAM lparam);

  CString downloaded_file();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadConfigDlg);

  // Closes the dialog.
  void CloseDialog();

  // URL of the file to download.
  CString url_;

  // HTTP download operation object.
  HttpDownloadOperation<DownloadConfigDlg>* download_op_;

  // Full path to the downloaded file.
  CString downloaded_file_;
};

#endif  // CHROMEOSRECOVERY_DOWNLOADCONFIGDLG_H_
