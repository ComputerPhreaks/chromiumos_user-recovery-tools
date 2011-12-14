// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "config_data.h"
#include "download_page.h"
#include "http_download_operation.h"
#include "unzip_file_operation.h"
#include "utils.h"
#include "verify_image_operation.h"

#define CROS_DOWNLOAD_PROGRESS_START    0
#define CROS_DOWNLOAD_PROGRESS_END      70
#define CROS_VERIFY_PROGRESS_START      70
#define CROS_VERIFY_PROGRESS_END        75
#define CROS_UNZIP_PROGRESS_START       75
#define CROS_UNZIP_PROGRESS_END         100

// DownloadPage dialog

DownloadPage::DownloadPage(__in ChromeOSRecoveryWizard* parent)
    : CPropertyPageImpl<DownloadPage>(IDS_CHROMEOSRECOVERY),
      parent_(parent) {
  curr_operation_ = NULL;
}

DownloadPage::~DownloadPage() {
  if (curr_operation_)
    delete curr_operation_;
  if (!downloaded_file_.IsEmpty())
    ::DeleteFile(downloaded_file_);
}

// Message handlers

BOOL DownloadPage::OnInitDialog(__in HWND focus, __in LPARAM param) {
  ProgressDlg::OnInitDialog(focus, param);

  CString message;
  message.LoadString(IDS_DOWNLOADING_RECOVERY_IMAGE);
  SetDlgItemText(IDC_MESSAGE, message);

  return TRUE;
}

LRESULT DownloadPage::OnDownloadDone(__in UINT /*msg*/,
                                     __in WPARAM /*wparam*/,
                                     __in LPARAM /*lparam*/) {
  DWORD error = ERROR_SUCCESS;
  HttpDownloadOperation<DownloadPage>* download_op =
    dynamic_cast<HttpDownloadOperation<DownloadPage>*>(curr_operation_);
  CString hash;
  VerifyImageOperation<DownloadPage>::HASH_ALG hash_alg;

  error = last_dlg_error();

  if (cancel_requested_) {
    _Module.Log(CROS_LOG_INFO, L"Download cancelled");
    goto done;
  }

  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR, L"Download failed (error=%d)", error);
    goto done;
  }

  downloaded_file_ = download_op->GetDownloadedFile();

  _Module.Log(CROS_LOG_INFO,
              L"Download completed, file downloaded to %s",
              downloaded_file_);

  VerifyImageOperation<DownloadPage>* verify_op =
    new(std::nothrow) VerifyImageOperation<DownloadPage>(
        this,
        WM_CROS_VERIFY_DONE,
        CROS_VERIFY_PROGRESS_START,
        CROS_VERIFY_PROGRESS_END);
  if (!verify_op) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create verify image operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  if (!device_->sha1().IsEmpty()) {
    hash = device_->sha1();
    hash_alg = VerifyImageOperation<DownloadPage>::kSha1;
  } else {
    hash = device_->md5();
    hash_alg = VerifyImageOperation<DownloadPage>::kMd5;
  }

  error = verify_op->Init(downloaded_file_,
                          device_->zipfile_size(),
                          hash,
                          hash_alg);
  if (error != ERROR_SUCCESS) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to initialize verify image operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  _Module.Log(CROS_LOG_INFO, L"Starting verify");
  error = verify_op->Start();
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to start verify image operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  if (curr_operation_)
    delete curr_operation_;
  curr_operation_ = verify_op;

 done:
  set_last_dlg_error(error);
  if (error != ERROR_SUCCESS)
    OnOperationCancelled();
  return 0;
}

LRESULT DownloadPage::OnVerifyDone(__in UINT /*msg*/,
                                   __in WPARAM /*wparam*/,
                                   __in LPARAM /*lparam*/) {
  DWORD error = ERROR_SUCCESS;
  CString extracted_image_file;

  error = last_dlg_error();

  if (cancel_requested_) {
    _Module.Log(CROS_LOG_INFO, L"Verify cancelled");
    goto done;
  }

  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR, L"Verify image failed (error=%d)", error);
    goto done;
  }

  _Module.Log(CROS_LOG_INFO, L"Verify completed");

  UnzipFileOperation<DownloadPage>* unzip_op =
    new(std::nothrow) UnzipFileOperation<DownloadPage>(
        this,
        WM_CROS_UNZIP_DONE,
        CROS_UNZIP_PROGRESS_START,
        CROS_UNZIP_PROGRESS_END);
  if (!unzip_op) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create unzip file operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  error = unzip_op->Init(device_->file(),
                         device_->file_size(),
                         downloaded_file_,
                         &extracted_image_file);
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to initialize unzip operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  _Module.Log(CROS_LOG_INFO, L"Starting unzip");
  error = unzip_op->Start();
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to start unzip operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  delete curr_operation_;
  curr_operation_ = unzip_op;

 done:
  set_last_dlg_error(error);
  if (error != ERROR_SUCCESS)
    OnOperationCancelled();
  return 0;
}

LRESULT DownloadPage::OnUnzipDone(__in UINT /*msg*/,
                                  __in WPARAM /*wparam*/,
                                  __in LPARAM /*lparam*/) {
  UnzipFileOperation<DownloadPage>* unzip_op =
    dynamic_cast<UnzipFileOperation<DownloadPage>*>(curr_operation_);
  CString zip_file_name = downloaded_file_;

  DWORD error = last_dlg_error();

  if (cancel_requested_) {
    _Module.Log(CROS_LOG_INFO, L"Unzip cancelled");
    OnOperationCancelled();
    goto done;
  }

  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR, L"Download failed (error=%d)", error);
    goto done;
  }

  // Make the downloaded file be the unzipped image file.
  downloaded_file_ = unzip_op->GetUnzippedFile();

  _Module.Log(CROS_LOG_INFO,
              L"Unzip completed, file unzipped to %s",
              downloaded_file_);

  delete curr_operation_;
  curr_operation_ = NULL;

  ::DeleteFile(zip_file_name);

  parent_->SetActivePage(ChromeOSRecoveryWizard::kWriteImagePageIndex);

 done:
  return 0;
}

// Notifications

int DownloadPage::OnSetActive() {
  DWORD error = ERROR_SUCCESS;
  HttpDownloadOperation<DownloadPage>* download_op = NULL;
  Device::URL_LIST::iterator it;

  parent_->SetWizardButtons(0);

  Reset();

  device_ = parent_->select_device_page()->GetSelectedDevice();
  if(!device_)
    goto done;

  // Start downloading the image by trying each url until we get one
  // that succeeds.
  for (it = device_->urls()->begin();
       it != device_->urls()->end();
       ++it) {
    download_op = new(std::nothrow) HttpDownloadOperation<DownloadPage>(
                      this,
                      WM_CROS_DOWNLOAD_DONE,
                      CROS_DOWNLOAD_PROGRESS_START,
                      CROS_DOWNLOAD_PROGRESS_END);
    if (!download_op) {
      error = ERROR_OUTOFMEMORY;
      _Module.Log(CROS_LOG_ERROR,
                  L"Failed to create HTTP download operation (%d)",
                  error);
      DisplayInternalErrorMessage();
      goto done;
    }

    error = download_op->Init(*it);
    if (error != ERROR_SUCCESS) {
      goto done;
    }

    _Module.Log(CROS_LOG_INFO, L"Downloading recovery image from %s", *it);
    error = download_op->Start();
    if (error == ERROR_SUCCESS) {
      curr_operation_ = download_op;
      goto done;
    } else {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to download from %s (%d)",
                  *it,
                  error);
      delete download_op;
      download_op = NULL;
    }
  }

 done:
  set_last_dlg_error(error);
  if (error != ERROR_SUCCESS && !download_op)
    delete download_op;
  return 0;
}

BOOL DownloadPage::OnQueryCancel() {
  int ret = FmtMessageBox(m_hWnd,
                          MB_YESNO | MB_ICONQUESTION |
                          MB_APPLMODAL | MB_SETFOREGROUND,
                          IDS_CONFIRM_CANCEL);
  if (ret == IDYES) {
      cancel_requested_ = TRUE;
      if (curr_operation_)
        curr_operation_->Cancel();
  }

  return TRUE;
}

// Accessors and mutators

CString DownloadPage::downloaded_file() {
  return downloaded_file_;
}

// Private methods

void DownloadPage::Reset() {
  set_status(L"");
  set_progress(0);

  cancel_requested_ = FALSE;
  if (curr_operation_) {
    delete curr_operation_;
    curr_operation_ = NULL;
  }
  if (!downloaded_file_.IsEmpty())
    ::DeleteFile(downloaded_file_);
  downloaded_file_.Empty();
}

void DownloadPage::OnOperationCancelled() {
  parent_->SetActivePage(ChromeOSRecoveryWizard::kSelectUSBPageIndex);
}
