// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_WRITEIMAGEPAGE_H_
#define CHROMEOSRECOVERY_WRITEIMAGEPAGE_H_

#include "progress_dlg.h"

class ChromeOSRecoveryWizard;
class CDevice;
template<class T> class WriteImageOperation;

// Wizard page that writes an image file to the selected USB device while
// displaying progress to the user.
class WriteImagePage : public CPropertyPageImpl<WriteImagePage>,
                       public ProgressDlg<WriteImagePage> {
 public:
  enum { IDD = IDD_PROPPAGE_PROGRESS };

  explicit WriteImagePage(__in ChromeOSRecoveryWizard* parent);
  virtual ~WriteImagePage();

  BEGIN_MSG_MAP(WriteImagePage)
    MSG_WM_INITDIALOG(OnInitDialog)
    MESSAGE_HANDLER_EX(WM_CROS_WRITE_IMAGE_DONE, OnWriteImageDone)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancelInternal)
    CHAIN_MSG_MAP(CPropertyPageImpl<WriteImagePage>)
  END_MSG_MAP()

  // WTL calls this method to initialize the page.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this method when the write image operation completes.
  LRESULT OnWriteImageDone(__in UINT msg,
                           __in WPARAM wparam,
                           __in LPARAM lparam);

  // WTL calls this method when the page is set active.
  int OnSetActive();

  // WTL calls this method when the user clicks on the Cancel button to see if
  // it's OK to cancel this page.
  BOOL OnQueryCancel();

 private:
  // Resets the state of this page.
  void Reset();

  // The base calls ProgressDlg calls this method when the write image
  // operation is cancelled.
  void OnOperationCancelled();

  // Pointer to the wizard that hosts this page.
  ChromeOSRecoveryWizard* parent_;

  // Pointer to the write image operation.
  WriteImageOperation<WriteImagePage>* write_image_op_;

  DISALLOW_COPY_AND_ASSIGN(WriteImagePage);
};

#endif  // CHROMEOSRECOVERY_WRITEIMAGEPAGE_H_
