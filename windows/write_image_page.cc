// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "config_data.h"
#include "utils.h"
#include "write_image_page.h"
#include "write_image_operation.h"

// WriteImagePage dialog

WriteImagePage::WriteImagePage(__in ChromeOSRecoveryWizard* parent)
    : CPropertyPageImpl<WriteImagePage>(IDS_CHROMEOSRECOVERY),
      parent_(parent) {
  write_image_op_ = NULL;
}

WriteImagePage::~WriteImagePage() {
  if (write_image_op_)
    delete write_image_op_;
}

// Message handlers

BOOL WriteImagePage::OnInitDialog(__in HWND focus, __in LPARAM param) {
  ProgressDlg::OnInitDialog(focus, param);

  CString message;
  message.LoadString(IDS_WRITING_RECOVERY_IMAGE);
  SetDlgItemText(IDC_MESSAGE, message);

  return TRUE;
}

LRESULT WriteImagePage::OnWriteImageDone(__in UINT /*msg*/,
                                         __in WPARAM /*wparam*/,
                                         __in LPARAM /*lparam*/) {
  DWORD error = ERROR_SUCCESS;

  ATLASSERT(write_image_op_ != NULL);
  delete write_image_op_;
  write_image_op_ = NULL;

  error = last_dlg_error();

  if (cancel_requested_) {
    _Module.Log(CROS_LOG_INFO, L"Write image cancelled");
    goto done;
  }

  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR, L"Write image failed (error=%d)", error);
    goto done;
  }

  _Module.Log(CROS_LOG_INFO, L"Write image completed");

  parent_->SetActivePage(ChromeOSRecoveryWizard::kFinishPageIndex);

 done:
  set_last_dlg_error(error);
  if (error != ERROR_SUCCESS)
    OnOperationCancelled();
  return 0;
}

// Notifications

int WriteImagePage::OnSetActive() {
  DWORD error = ERROR_SUCCESS;
  CString image_file_name;
  CWaitCursor wait;

  SetWizardButtons(0);

  Reset();

  write_image_op_ = new(std::nothrow) WriteImageOperation<WriteImagePage>(
                        this,
                        WM_CROS_WRITE_IMAGE_DONE,
                        0,
                        100);
  if (!write_image_op_) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create write operation (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  SelectDevicePage* select_device_page = parent_->select_device_page();
  DownloadPage* download_page = parent_->download_page();
  Device* device = select_device_page->GetSelectedDevice();
  if (!device)
    image_file_name = select_device_page->GetSelectedDeviceName();
  else
    image_file_name = download_page->downloaded_file();

  ULONGLONG image_file_size = 0;
  error = GetFileSize(image_file_name, &image_file_size);
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to query file '%s' size (%d)",
                image_file_name,
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  SelectUSBPage* select_usb_page = parent_->select_usb_page();
  error = write_image_op_->Init(image_file_name,
                                 image_file_size,
                                 select_usb_page->selected_usb_disk_index());
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to initialize write operation (%d)",
                error);
    goto done;
  }

  _Module.Log(CROS_LOG_INFO,
              L"Writing image %s to \\\\.\\PhysicalDrive%d",
              image_file_name,
              select_usb_page->selected_usb_disk_index());
  error = write_image_op_->Start();
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to start write operation (%d)",
                error);
    goto done;
  }

 done:
  if (error != ERROR_SUCCESS)
    return IDD_PROPPAGE_SELECT_USB;
  else
    return 0;
}

BOOL WriteImagePage::OnQueryCancel() {
  int ret = FmtMessageBox(m_hWnd,
                          MB_YESNO | MB_ICONQUESTION |
                          MB_APPLMODAL | MB_SETFOREGROUND,
                          IDS_CONFIRM_CANCEL);
  if (ret == IDYES) {
    cancel_requested_ = TRUE;
    write_image_op_->Cancel();
  }

  return TRUE;
}

// Private methods

void WriteImagePage::Reset() {
  set_status(L"");
  set_progress(0);
  cancel_requested_ = FALSE;
}

void WriteImagePage::OnOperationCancelled() {
  parent_->SetActivePage(ChromeOSRecoveryWizard::kSelectUSBPageIndex);
}
