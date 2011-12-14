// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_WRITEIMAGEOPERATION_H_
#define CHROMEOSRECOVERY_WRITEIMAGEOPERATION_H_

#include "operation.h"
#include "unzip.h"
#include "utils.h"

#define WRITEIMAGEDLG_WRITE_SIZE 0x100000 // 1MB

// Operation that writes an image file to a USB device.
template <class T>
class WriteImageOperation : public Operation<T> {
 public:
  WriteImageOperation(__in T* dlg,
                      __in UINT done_msg,
                      __in int progress_start,
                      __in int progress_end);
  virtual ~WriteImageOperation();

  // Initializes the write image operation.
  DWORD Init(__in const CString& image_file_name,
             __in ULONGLONG image_file_size,
             __in DWORD usb_disk_index);

  // Starts the write image operation.
  virtual DWORD Start();

  // Cancels the write image operation.
  virtual void Cancel();

 private:
  // Writes the image to the removable media.
  DWORD WriteImage();

  // Write image thread.
  static DWORD WINAPI WriteImageThread(__in LPVOID param);

  // Full path to the image to write.
  CString image_file_name_;

  // Handle to the image file.
  CAtlFile image_file_;

  // Size of the image file in bytes.
  ULONGLONG image_file_size_;

  // Full path to the USB device name that will receive the image.
  CString usb_device_name_;

  // Handle to the USB device.
  CAtlFile usb_device_;

  // Sector size of the USB device in bytes.
  DWORD usb_sector_size_;

  // Buffer that is used to read/write the image file.
  LPBYTE buffer_;
  DWORD buffer_size_;

  // Handle to the write image thread.
  HANDLE write_image_thread_;

  DISALLOW_COPY_AND_ASSIGN(WriteImageOperation);
};

template<class T>
WriteImageOperation<T>::WriteImageOperation(__in T* dlg,
                                            __in UINT done_msg,
                                            __in int progress_start,
                                            __in int progress_end)
    : Operation(dlg, done_msg, progress_start, progress_end) {
  usb_sector_size_ = 512;
  buffer_ = NULL;
  buffer_size_ = 0;
  write_image_thread_ = NULL;
}

template<class T>
WriteImageOperation<T>::~WriteImageOperation() {
  if (write_image_thread_) {
    WaitForSingleObject(write_image_thread_, INFINITE);
    CloseHandle(write_image_thread_);
  }
  if (buffer_)
    VirtualFree(buffer_, 0, MEM_RELEASE);
}

template<class T>
DWORD WriteImageOperation<T>::Init(__in const CString& image_file_name,
                                   __in ULONGLONG image_file_size,
                                   __in DWORD usb_disk_index) {
  DWORD error = ERROR_SUCCESS;
  HANDLES volume_handles;

  image_file_name_ = image_file_name;
  image_file_size_ = image_file_size;

  HRESULT hr = image_file_.Create(image_file_name_,
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   OPEN_EXISTING,
                                   0,
                                   NULL,
                                   NULL);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_OPEN_FILE,
                  image_file_name_,
                  error);
    goto done;
  }

  error = DismountUsbDiskVolumes(usb_disk_index, &volume_handles);
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR, L"Failed to dismount USB volumes (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  usb_device_name_.Format(L"\\\\.\\PhysicalDrive%d", usb_disk_index);
  hr = usb_device_.Create(usb_device_name_,
                          GENERIC_WRITE,
                          0,
                          OPEN_EXISTING,
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                          NULL,
                          NULL);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_OPEN_FILE,
                  usb_device_name_,
                  error);
    goto done;
  }

  // Use VirtualAlloc() to get an aligned buffer since we're opening the
  // USB device DASD
  buffer_size_ = WRITEIMAGEDLG_WRITE_SIZE;
  buffer_ = (LPBYTE) VirtualAlloc(NULL,
                                  buffer_size_,
                                  MEM_COMMIT,
                                  PAGE_READWRITE);
  if (!buffer_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate buffer used for image writing (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  HANDLES::iterator it;
  for (it = volume_handles.begin(); it != volume_handles.end(); ++it)
    CloseHandle(*it);
  return error;
}

template<class T>
DWORD WriteImageOperation<T>::Start() {
  DWORD error = ERROR_SUCCESS;

  write_image_thread_ = CreateThread(NULL,
                                     0,
                                     WriteImageThread,
                                     this,
                                     0,
                                     NULL);
  if (!write_image_thread_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create write image thread (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  return error;
}

template<class T>
void WriteImageOperation<T>::Cancel() {
  TerminateThread(write_image_thread_, ERROR_OPERATION_ABORTED);
  WaitForSingleObject(write_image_thread_, INFINITE);
  dlg_->set_last_dlg_error(ERROR_CANCELLED);
  dlg_->PostMessage(done_msg_);
}

// Private methods

template<class T>
DWORD WriteImageOperation<T>::WriteImage() {
  DWORD error;
  LARGE_INTEGER offset = {0};
  CString bytes_written_str;
  CString image_file_size_str;

  OVERLAPPED overlap;
  ZeroMemory(&overlap, sizeof(overlap));

  while (TRUE) {
    overlap.Offset = offset.LowPart;
    overlap.OffsetHigh = offset.HighPart;

    DWORD bytes_xferred;
    BOOL success = ReadFile(image_file_,
                            buffer_,
                            buffer_size_,
                            &bytes_xferred,
                            &overlap);
    error = GetLastError();
    if (!success && error != ERROR_HANDLE_EOF) {
      _Module.Log(CROS_LOG_ERROR,
                  L"Failed to read file %s "
                  L"(buffer=0x%p, offset=%I64d, num_bytes=%d, error=%d)",
                  image_file_name_,
                  buffer_,
                  offset.QuadPart,
                  buffer_size_,
                  error);
      FmtMessageBox(dlg_->m_hWnd,
                    MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                    IDS_ERROR_FAILED_READ_FILE,
                    image_file_name_,
                    error);
      goto done;
    }

    if (error == ERROR_HANDLE_EOF || !bytes_xferred) {
      error = ERROR_SUCCESS;
      break;
    }

    success = WriteFile(usb_device_,
                        buffer_,
                        bytes_xferred,
                        &bytes_xferred,
                        &overlap);
    if (!success) {
      error = GetLastError();
      _Module.Log(CROS_LOG_ERROR,
                  L"Failed to write file %s "
                  L"(buffer=0x%p, offset=%I64d, num_bytes=%d, error=%d)",
                  image_file_name_,
                  buffer_,
                  offset.QuadPart,
                  buffer_size_,
                  error);
      FmtMessageBox(dlg_->m_hWnd,
                    MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                    IDS_ERROR_FAILED_WRITE_FILE,
                    usb_device_name_,
                    error);
      goto done;
    }

    offset.QuadPart += bytes_xferred;
 
    int progress = CalcProgress(offset.QuadPart + 1, image_file_size_);
    dlg_->set_progress(progress);
 
    bytes_written_str = BytesToStr(offset.QuadPart + 1);
    image_file_size_str = BytesToStr(image_file_size_);
    dlg_->set_status(IDS_STATUS_WRITING,
                     bytes_written_str,
                     image_file_size_str);
  }

  dlg_->set_progress(100);
  error = ERROR_SUCCESS;
  Sleep(1000);  // Let the UI catchup and display 100%

 done:
  dlg_->set_last_dlg_error(error);
  dlg_->PostMessage(done_msg_);
  return error;
}

template<class T>
DWORD WINAPI WriteImageOperation<T>::WriteImageThread(__in LPVOID param) {
  WriteImageOperation<T>* _this =
      reinterpret_cast<WriteImageOperation<T>*>(param);
  SetThreadUILanguage(_Module.ui_langid());
  return _this->WriteImage();
}

#endif  // CHROMEOSRECOVERY_WRITEIMAGEOPERATION_H_
