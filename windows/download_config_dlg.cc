// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "config_data.h"
#include "download_config_dlg.h"
#include "http_download_operation.h"
#include "utils.h"

DownloadConfigDlg::DownloadConfigDlg(__in const CString& url) : url_(url) {
  download_op_ = NULL;
}

DownloadConfigDlg::~DownloadConfigDlg() {
  if (download_op_)
    delete download_op_;
  if (last_dlg_error() != ERROR_SUCCESS && downloaded_file_.GetLength() > 0)
    ::DeleteFile(downloaded_file_);
}

// Message handlers

BOOL DownloadConfigDlg::OnInitDialog(__in HWND focus, __in LPARAM param) {
  DWORD error = ERROR_GEN_FAILURE;

  ProgressDlg::OnInitDialog(focus, param);

  _Module.Log(CROS_LOG_INFO, L"Downloading config");

  download_op_ = new(std::nothrow) HttpDownloadOperation<DownloadConfigDlg>(
                     this,
                     WM_CROS_DOWNLOAD_DONE,
                     0,
                     100);
  if (!download_op_) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create HTTP download operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  error = download_op_->Init(url_);
  if (error != ERROR_SUCCESS) {
    goto done;
  }

  error = download_op_->Start();

 done:
  set_last_dlg_error(error);
  if (error != ERROR_SUCCESS && !download_op_) {
    delete download_op_;
    download_op_ = NULL;
  }
  return (error == ERROR_SUCCESS);
}

void DownloadConfigDlg::OnCancel(__in UINT code, __in int id, __in HWND ctrl) {
  CWaitCursor wait_cursor;
  cancel_requested_ = TRUE;
  if (download_op_)
    download_op_->Cancel();
}

LRESULT DownloadConfigDlg::OnDownloadDone(__in UINT /*msg*/,
                                          __in WPARAM /*wparam*/,
                                          __in LPARAM /*lparam*/) {
  if (cancel_requested_) {
    _Module.Log(CROS_LOG_INFO, L"Downloading config cancelled");
    goto done;
  }

  downloaded_file_ = download_op_->GetDownloadedFile();

  _Module.Log(CROS_LOG_INFO,
              L"Downloading config completed, config file %s",
              downloaded_file_);

 done:
  CloseDialog();
  return 0;
}

// Accessors and mutators

CString DownloadConfigDlg::downloaded_file() {
  return downloaded_file_;
}

// Private methods

void DownloadConfigDlg::CloseDialog() {
  if (!IsWindow())
    return;

  if (cancel_requested_)
    set_last_dlg_error(ERROR_CANCELLED);

  DWORD error = last_dlg_error();
  switch (error) {
    case ERROR_SUCCESS:
      EndDialog(IDOK);
      break;
    case ERROR_CANCELLED:
      EndDialog(IDCANCEL);
      break;
    default:
      EndDialog(IDABORT);
  }
}
