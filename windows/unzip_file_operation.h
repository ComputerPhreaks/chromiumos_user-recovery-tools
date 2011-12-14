// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_UNZIPFILEOPERATION_H_
#define CHROMEOSRECOVERY_UNZIPFILEOPERATION_H_

#include "operation.h"
#include "unzip.h"

// Operation that unzips the specified file.
template <class T>
class UnzipFileOperation : public Operation<T> {
 public:
  UnzipFileOperation(__in T* dlg,
                     __in UINT done_msg,
                     __in int progress_start,
                     __in int progress_end);
  virtual ~UnzipFileOperation();

  // Initializes the unzip file operation.
  // This operation unzips |image_file_name| from |image_zipfile|.
  DWORD Init(__in const CString& image_file_name,
             __in ULONGLONG image_file_size,
             __in const CString& image_zipfile,
             __deref_out CString* image_file);

  // Starts the unzip file operation.
  virtual DWORD Start();

  // Cancels the unzip file operation.
  virtual void Cancel();

  // Returns the full path to the unzipped file.
  CString GetUnzippedFile();

 private:
  // Thread that unzips the file.
  static DWORD WINAPI UnzipThread(__in LPVOID param);

  // Because the unzip library is synchronous, we use this separate thread to
  // monitor the progress of the unzip operation.
  static DWORD WINAPI UnzipProgressThread(__in LPVOID param);

  // The name of the image file in the zip file.
  CString image_file_name_;

  // The size, in bytes, of the image file.
  ULONGLONG image_file_size_;

  // Full path to the zip file that contains the image file.
  CString image_zipfile_;

  // Temporary file used to hold the unzipped image file.  This gets converted
  // to a regular file after the operation completes successfully.
  CAtlTemporaryFile temp_file_;

  // Handle to the unzip request.
  HZIP hz_;

  // TRUE if the unzip operation is currently in progress.
  BOOL unzip_in_progress_;

  // Handle to the unzip thread.
  HANDLE unzip_thread_;

  // Handle to the timer that periodically wakes and checks on the unzip
  // progress. 
  HANDLE progress_update_timer_;

  // Handle to the thread that checks on the unzip progress.
  HANDLE unzip_progress_thread_;

  DISALLOW_COPY_AND_ASSIGN(UnzipFileOperation);
};

template<class T>
UnzipFileOperation<T>::UnzipFileOperation(__in T* dlg,
                                          __in UINT done_msg,
                                          __in int progress_start,
                                          __in int progress_end)
    : Operation(dlg, done_msg, progress_start, progress_end) {
  image_file_size_ = 0;
  hz_ = NULL;
  unzip_in_progress_ = FALSE;
  unzip_thread_ = NULL;
  progress_update_timer_ = NULL;
}

template<class T>
UnzipFileOperation<T>::~UnzipFileOperation() {
  if (unzip_progress_thread_) {
    TerminateThread(unzip_progress_thread_, ERROR_OPERATION_ABORTED);
    WaitForSingleObject(unzip_progress_thread_, INFINITE);
    CloseHandle(unzip_progress_thread_);
  }
  if (unzip_thread_) {
    WaitForSingleObject(unzip_thread_, INFINITE);
    CloseHandle(unzip_thread_);
  }
  if (progress_update_timer_)
    CloseHandle(progress_update_timer_);
  if (hz_) {
    CloseZip(hz_);
  }
}

template<class T>
DWORD UnzipFileOperation<T>::Init(__in const CString& image_file_name,
                                  __in ULONGLONG image_file_size,
                                  __in const CString& image_zipfile,
                                  __deref_out CString* image_file) {
  DWORD error = ERROR_SUCCESS;

  image_file_name_ = image_file_name;
  image_file_size_ = image_file_size;
  image_zipfile_ = image_zipfile;

  HRESULT hr = temp_file_.Create(NULL, GENERIC_READ | GENERIC_WRITE);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_OPEN_FILE,
                  temp_file_.TempFileName(),
                  error);
    goto done;
  }

  hz_ = OpenZip(image_zipfile_, NULL);
  if (!hz_) {
    error = ERROR_FILE_NOT_FOUND;
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_OPEN_FILE,
                  image_zipfile_,
                  error);
    goto done;
  }

  progress_update_timer_ = CreateWaitableTimer(NULL, FALSE, NULL);
  if (!progress_update_timer_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create unzip progress update timer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  LONG timer_period = 300;
  LARGE_INTEGER due_time;
  due_time.QuadPart = 0 - (static_cast<LONGLONG>(timer_period) * 10000LL);
  BOOL success = SetWaitableTimer(progress_update_timer_,
                                  &due_time,
                                  timer_period,
                                  NULL,
                                  NULL,
                                  FALSE);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to set unzip progress update timer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  return error;
}

template<class T>
DWORD UnzipFileOperation<T>::Start() {
  DWORD error = ERROR_SUCCESS;

  unzip_thread_ = CreateThread(NULL,
                                0,
                                UnzipFileOperation::UnzipThread,
                                this,
                                0,
                                NULL);
  if (!unzip_thread_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to start unzip thread (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  unzip_progress_thread_ = CreateThread(
                               NULL,
                               0,
                               UnzipFileOperation::UnzipProgressThread,
                               this,
                               0,
                               NULL);
  if (!unzip_progress_thread_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to start unzip progress thread (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  return error;
}

template<class T>
void UnzipFileOperation<T>::Cancel() {
  TerminateThread(unzip_progress_thread_, ERROR_OPERATION_ABORTED);
  WaitForSingleObject(unzip_progress_thread_, INFINITE);
  TerminateThread(unzip_thread_, ERROR_OPERATION_ABORTED);
  WaitForSingleObject(unzip_thread_, INFINITE);
  dlg_->PostMessage(done_msg_);
}

template<class T>
CString UnzipFileOperation<T>::GetUnzippedFile() {
  CString file_name = GetTempFileName();
  temp_file_.Close(file_name);
  return file_name;
}

// Private methods

template<class T>
DWORD WINAPI UnzipFileOperation<T>::UnzipThread(__in LPVOID param) {
  DWORD error;
  UnzipFileOperation* op = reinterpret_cast<UnzipFileOperation*>(param);
  T* dlg = op->dlg_;

  SetThreadUILanguage(_Module.ui_langid());

  int index;
  ZIPENTRY entry;
  ZRESULT zr = FindZipItem(op->hz_,
                           op->image_file_name_,
                           true,
                           &index,
                           &entry);
  if (zr != ZR_OK) {
    error = zr;
    FmtMessageBox(dlg->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_CANNOT_FIND_FILE_IN_ZIP,
                  op->image_file_name_,
                  error);
    goto done;
  }

  op->unzip_in_progress_ = TRUE;

  zr = UnzipItemHandle(op->hz_, index, (HANDLE) op->temp_file_);
  if (zr != ZR_OK) {
    error = zr;
    FmtMessageBox(dlg->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_UNZIP_FILE,
                  op->image_zipfile_,
                  error);
    goto done;
  }

  TerminateThread(op->unzip_progress_thread_, ERROR_SUCCESS);

  dlg->set_progress(op->progress_end_);
  dlg->set_status(IDS_STATUS_UNZIP_COMPLETE);
  Sleep(1000);  // Let the UI catchup and display 100%

  error = ERROR_SUCCESS;

 done:
  dlg->set_last_dlg_error(error);
  dlg->PostMessage(op->done_msg_);

  return error;
}

template<class T>
DWORD WINAPI UnzipFileOperation<T>::UnzipProgressThread(__in LPVOID param) {
  UnzipFileOperation* op = reinterpret_cast<UnzipFileOperation*>(param);
  T* dlg = op->dlg_;

  SetThreadUILanguage(_Module.ui_langid());

  while (TRUE) {
    DWORD wait_result = WaitForSingleObject(op->progress_update_timer_,
                                            INFINITE);
    if (wait_result != WAIT_OBJECT_0)
      break;

    if (!op->unzip_in_progress_)
      continue;

    // Use the current file size to determine unzip status
    ULONGLONG size = 0;
    HRESULT hr = op->temp_file_.GetSize(size);
    if (FAILED(hr))
      continue;

    int progress = op->CalcProgress(size, op->image_file_size_);
    dlg->set_progress(progress);

    CString curr_size_str = BytesToStr(size);
    CString image_file_size_str = BytesToStr(op->image_file_size_);
    dlg->set_status(IDS_STATUS_UNZIPPING, curr_size_str, image_file_size_str);
  }

  return 0;
}

#endif  // CHROMEOSRECOVERY_UNZIPFILEOPERATION_H_
