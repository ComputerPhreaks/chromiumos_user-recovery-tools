// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_VERIFYIMAGEOPERATION_H_
#define CHROMEOSRECOVERY_VERIFYIMAGEOPERATION_H_

#include "operation.h"

// Operation that verifies an image against its MD5/SHA1 hash.
template<class T>
class VerifyImageOperation : public Operation<T> {
 public:
  typedef enum _HASH_ALG {
    kMd5,
    kSha1
  } HASH_ALG;
 
  VerifyImageOperation(__in T* dlg,
                       __in UINT done_msg,
                       __in int progress_start,
                       __in int progress_end);
  virtual ~VerifyImageOperation();

  // Initializes the verify image operation.
  DWORD Init(__in const CString& image_file,
             __in ULONGLONG image_file_size,
             __in const CString& hash,
             __in HASH_ALG hash_alg);

  // Starts the verify image operation.
  virtual DWORD Start();

  // Cancels the verify image operation.
  virtual void Cancel();

 private:
  // Thread that verifies the image file against its hash.
  static DWORD WINAPI VerifyThread(__in LPVOID param);

  // Returns TRUE if the hash is valid.
  BOOL IsHashValid(__in_bcount(hash_size) LPBYTE hash, __in SIZE_T hash_size);

  // Specifies the hash algorithm to use for verification.
  HASH_ALG hash_alg_;

  // Full path to the image file.
  CString image_file_name_;

  // Size, in bytes, of the image file.
  ULONGLONG image_file_size_;

  // Hash to verify against.
  CString hash_value_;

  // Handle to the verify thread.
  HANDLE verify_thread_;

  // Image file handle.
  CAtlFile image_file_;

  // Hash crypto handles.
  HCRYPTPROV crypt_prov_;
  HCRYPTHASH crypt_hash_;

  // Buffer used to read in the content of the image file.
  LPBYTE buffer_;
  DWORD buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(VerifyImageOperation);
};

#define VERIFYIMAGEDLG_READ_BUFFER_SIZE 65536

template<class T>
VerifyImageOperation<T>::VerifyImageOperation(__in T* dlg,
                                              __in UINT done_msg,
                                              __in int progress_start,
                                              __in int progress_end)
    : Operation(dlg, done_msg, progress_start, progress_end) {
  verify_thread_ = NULL;
  crypt_prov_ = NULL;
  crypt_hash_ = NULL;
  buffer_ = NULL;
}

template<class T>
VerifyImageOperation<T>::~VerifyImageOperation() {
  if (verify_thread_) {
    WaitForSingleObject(verify_thread_, INFINITE);
    CloseHandle(verify_thread_);
  }
  if (crypt_hash_)
    CryptDestroyHash(crypt_hash_);
  if (crypt_prov_)
    CryptReleaseContext(crypt_prov_, 0);
  if (buffer_)
    LocalFree(buffer_);
}

template<class T>
DWORD VerifyImageOperation<T>::Init(__in const CString& image_file,
                                    __in ULONGLONG image_file_size,
                                    __in const CString& hash,
                                    __in HASH_ALG hash_alg) {
  DWORD error = ERROR_SUCCESS;

  image_file_name_ = image_file;
  image_file_size_ = image_file_size;
  hash_value_ = hash;
  hash_alg_ = hash_alg;

  HRESULT hr = image_file_.Create(image_file_name_,
                                   GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
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

  BOOL success = CryptAcquireContext(&crypt_prov_,
                                     NULL,
                                     NULL,
                                     PROV_RSA_FULL,
                                     0);
  if (!success && GetLastError() == NTE_BAD_KEYSET) {
    success = CryptAcquireContext(&crypt_prov_,
                                  NULL,
                                  NULL,
                                  PROV_RSA_FULL,
                                  CRYPT_NEWKEYSET);
  }
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to acquire crypt context (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  ALG_ID algid = (hash_alg_ == kMd5) ? CALG_MD5 : CALG_SHA1;
  success = CryptCreateHash(crypt_prov_,
                            algid,
                            NULL,
                            0,
                            &crypt_hash_);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to create crypt hash (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  buffer_size_ = VERIFYIMAGEDLG_READ_BUFFER_SIZE;
  buffer_ = (LPBYTE) LocalAlloc(LPTR, buffer_size_);
  if (!buffer_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate verify read buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  dlg_->set_last_dlg_error(error);
  return error;
}

template<class T>
DWORD VerifyImageOperation<T>::Start() {
  DWORD error = ERROR_SUCCESS;

  verify_thread_ = CreateThread(NULL,
                                0,
                                VerifyThread,
                                this,
                                0,
                                NULL);
  if (!verify_thread_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create the verify thread (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  return error;
}

template <class T>
void VerifyImageOperation<T>::Cancel() {
  TerminateThread(verify_thread_, ERROR_OPERATION_ABORTED);
  WaitForSingleObject(verify_thread_, INFINITE);
  dlg_->set_last_dlg_error(ERROR_CANCELLED);
  dlg_->PostMessage(done_msg_);
}

// Private methods

template<class T>
DWORD WINAPI VerifyImageOperation<T>::VerifyThread(__in LPVOID param) {
  VerifyImageOperation* op = reinterpret_cast<VerifyImageOperation*>(param);
  T* dlg = op->dlg_;
  DWORD error;
  BOOL success;
  ULONGLONG bytes_processed = 0;

  SetThreadUILanguage(_Module.ui_langid());

  while (TRUE) {
    DWORD bytes_read;
    success = ReadFile(op->image_file_,
                       op->buffer_,
                       op->buffer_size_,
                       &bytes_read,
                       NULL);
    if (!success) {
      error = GetLastError();
      FmtMessageBox(dlg->m_hWnd,
                    MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                    IDS_ERROR_FAILED_READ_FILE,
                    op->image_file_name_,
                    error);
      goto done;
    }
    if (!bytes_read)
      break;

    success = CryptHashData(op->crypt_hash_, op->buffer_, bytes_read, 0);
    if (!success) {
      error = GetLastError();
      _Module.Log(CROS_LOG_ERROR, L"Failed to hash data (%d)", error);
      DisplayInternalErrorMessage();
      goto done;
    }

    bytes_processed += bytes_read;
    int progress = op->CalcProgress(bytes_processed, op->image_file_size_);
    dlg->set_progress(progress);
    
    CString bytes_processed_str = BytesToStr(bytes_processed);
    CString image_file_size_str = BytesToStr(op->image_file_size_);
    dlg->set_status(IDS_STATUS_VERIFYING,
                    bytes_processed_str,
                    image_file_size_str);
  }

  BYTE hash[32];
  DWORD hash_size = sizeof(hash);
  success = CryptGetHashParam(op->crypt_hash_,
                              HP_HASHVAL,
                              hash,
                              &hash_size,
                              0);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to get hash value (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  dlg->set_progress(op->progress_end_);
  Sleep(1000);

  if (op->IsHashValid(hash, hash_size)) {
    error = ERROR_SUCCESS;
  } else {
    error = ERROR_INVALID_DATA;
    FmtMessageBox(dlg->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_VERIFY,
                  error);
  }

 done:
  dlg->set_last_dlg_error(error);
  dlg->PostMessage(op->done_msg_);

  return error;
}

template<class T>
BOOL VerifyImageOperation<T>::IsHashValid(__in_bcount(hash_size) LPBYTE hash,
                                          __in SIZE_T hash_size) {
  // Convert hash into a string representation so we can compare it to the
  // value in the device configuration
  CString hash_str;
  CString hex;
  for (SIZE_T index = 0; index < hash_size; ++index) {
    hex.Format(L"%02x", hash[index]);
    hash_str += hex;
  }

  return (hash_str.CompareNoCase(hash_value_) == 0);
}

#endif  // CHROMEOSRECOVERY_VERIFYIMAGEOPERATION_H_
