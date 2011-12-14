// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_OPERATION_H_
#define CHROMEOSRECOVERY_OPERATION_H_

// Base class for all operations performed by the Download page/dialog.
// Possible operations are: download, verify, unzip and write image.
// The dialog starts the operation via Start().  When the operation completes,
// this class posts done_msg_ back to the dialog.  The dialog can cancel the
// operation via Cancel().
template<class T>
class Operation {
 public:
  Operation(T* dlg, UINT done_msg, int progress_start, int progress_end)
      : dlg_(dlg),
        done_msg_(done_msg),
        progress_start_(progress_start),
        progress_end_(progress_end) {};
  virtual ~Operation() {};

  // Starts the operation.
  virtual DWORD Start() = 0;

  // Cancels the operation.
  virtual void Cancel() = 0;

 protected:
  // Returns the percentage of completion.
  int CalcProgress(__in ULONGLONG curr, __in ULONGLONG total) {
    if (!total) {
      return 0;
    } else {
      ULONGLONG range = progress_end_ - progress_start_;
      return (int)((curr * range) / total) + progress_start_;
    }
  }

  // Dialog that owns the operation.
  T* dlg_;

  // Message that the operation posts back to the dialog when it completes.
  UINT done_msg_;

  // The operation start and end progress.  This allows multiple operations to
  // work together sequentially.  For example, download from 0-80, verify from
  // 80-90, unpack from 90-100.
  int progress_start_;
  int progress_end_;

  DISALLOW_COPY_AND_ASSIGN(Operation<T>);
};

#endif  // CHROMEOSRECOVERY_OPERATION_H_
