// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_HTTPDOWNLOADOPERATION_H_
#define CHROMEOSRECOVERY_HTTPDOWNLOADOPERATION_H_

#include "operation.h"
#include "utils.h"

#ifndef MUI_LANGUAGE_NAME
#define MUI_LANGUAGE_NAME     0x8
#endif
#ifndef MUI_THREAD_LANGUAGES
#define MUI_THREAD_LANGUAGES  0x40
#endif

// Operation that downloads a file via HTTP.
template<class T>
class HttpDownloadOperation : public Operation<T> {
 public:
  HttpDownloadOperation(__in T* dlg,
                        __in UINT done_msg,
                        __in int progress_start,
                        __in int progress_end);
  virtual ~HttpDownloadOperation();

  // Initializes the download operation.
  // |url| points to the file to download.
  DWORD Init(__in const CString& url);

  // Starts the download operation.
  virtual DWORD Start();

  // Cancels the download operation.
  virtual void Cancel();

  // Returns the full path to the downloaded file.
  CString GetDownloadedFile();

 private:
  // Saves the thread preferred UI languages.
  // If successful, this method allocates the |languages| buffer.
  BOOL SaveThreadPreferredUILanguages(__out WCHAR** languages,
                                      __out ULONG* num_languages);

  // Restores the thread preferred UI languages.
  // This method deletes the |languages| buffer.
  void RestoreThreadPreferredUILanguages(__in WCHAR* languages,
                                         __in ULONG num_languages);

  // WinHttp calls this method asynchronously as it downloads the file.
  static void CALLBACK WinHttpCallback(__in HINTERNET handle,
                                       __in DWORD_PTR context,
                                       __in DWORD code,
                                       __in void* info,
                                       __in DWORD length);

  // Handles the completion of the HTTP send request.
  void OnSendRequestComplete(__in HINTERNET request);

  // Handles when the HTTP response headers are available.
  void OnHeadersAvailable(__in HINTERNET request);

  // Handles the case when the HTTP request fails.
  void OnStatusRequestError(__in WINHTTP_ASYNC_RESULT* result);

  // Handles the completion of an HTTP read request.
  void OnReadComplete(__in HINTERNET request, __in DWORD length);

  // Ends the asynchronous HTTP operation.
  void EndAsyncHttpOps();

  // Function pointer to the Win32 API SetThreadPreferredUILanguages().
  typedef BOOL (WINAPI* SetThreadPreferredUILanguagesFunc)(
                   __in DWORD flags,
                   __in PCWSTR languages,
                   __out PULONG num_languages);
  SetThreadPreferredUILanguagesFunc set_thread_preferred_ui_languages_;

  // Function pointer to the Win32 API GetThreadPreferredUILanguages().
  typedef BOOL (WINAPI* GetThreadPreferredUILanguagesFunc)(
                   __in DWORD flags,
                   __out PULONG num_languages,
                   __out_opt PWSTR languages_buffer,
                   __inout PULONG languages_buffer_size);
  GetThreadPreferredUILanguagesFunc get_thread_preferred_ui_languages_;
 
  // Temporary file used to store the download file content.
  // We use a temporary file during the donwload.  If a failure is encountered,
  // then the file gets deleted automatically.  When the download completes
  // successfully, the file is made permanent.
  CAtlTemporaryFile file_;

  // Components of the URL to download from.
  URL_COMPONENTS url_components_;

  // Download buffer.
  LPBYTE buffer_;
  SIZE_T buffer_size_;

  // WinHttp handles used for download.
  HINTERNET session_;
  HINTERNET connection_;
  HINTERNET request_;

  // Progress information.
  DWORD total_download_size_;
  DWORD curr_download_size_;

  DISALLOW_COPY_AND_ASSIGN(HttpDownloadOperation<T>);
};

template<class T>
HttpDownloadOperation<T>::HttpDownloadOperation(__in T* dlg,
                                                __in UINT done_msg,
                                                __in int progress_start,
                                                __in int progress_end)
    : Operation(dlg, done_msg, progress_start, progress_end) {
  set_thread_preferred_ui_languages_ = NULL;
  get_thread_preferred_ui_languages_ = NULL;
  ZeroMemory(&url_components_, sizeof(url_components_));
  url_components_.dwStructSize = sizeof(url_components_);
  buffer_ = NULL;
  buffer_size_ = 0;
  session_ = NULL;
  connection_ = NULL;
  request_ = NULL;
  total_download_size_ = 0;
  curr_download_size_ = 0;
}

template<class T>
HttpDownloadOperation<T>::~HttpDownloadOperation() {
  if (request_)
    WinHttpCloseHandle(request_);
  if (connection_)
    WinHttpCloseHandle(connection_);
  if (session_)
    WinHttpCloseHandle(session_);
  if (buffer_)
    LocalFree(buffer_);
  if (url_components_.lpszHostName)
    LocalFree(url_components_.lpszHostName);
  if (url_components_.lpszUrlPath)
    LocalFree(url_components_.lpszUrlPath);
}

#define MAX_URL_LENGTH 2048

template<class T>
DWORD HttpDownloadOperation<T>::Init(__in const CString& url) {
  DWORD error = ERROR_SUCCESS;

  url_components_.dwHostNameLength = DNS_MAX_NAME_BUFFER_LENGTH;
  url_components_.lpszHostName = static_cast<WCHAR*>(LocalAlloc(LPTR,
                                     DNS_MAX_NAME_BUFFER_LENGTH *
                                     sizeof(WCHAR)));
  if (!url_components_.lpszHostName) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate host name buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  url_components_.dwUrlPathLength = MAX_URL_LENGTH;
  url_components_.lpszUrlPath = static_cast<WCHAR*>(LocalAlloc(LPTR,
                                    MAX_URL_LENGTH * sizeof(WCHAR)));
  if (!url_components_.lpszUrlPath) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate URL path buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  buffer_size_ = 65536;
  buffer_ = (LPBYTE) LocalAlloc(LPTR, buffer_size_);
  if (!buffer_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate download buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  BOOL success = WinHttpCrackUrl(url, 0, 0, &url_components_);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to crack URL (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  set_thread_preferred_ui_languages_ = 
      reinterpret_cast<SetThreadPreferredUILanguagesFunc>(GetProcAddress(
          GetModuleHandle(L"kernel32.dll"),
          "SetThreadPreferredUILanguages"));

  get_thread_preferred_ui_languages_ = 
      reinterpret_cast<GetThreadPreferredUILanguagesFunc>(GetProcAddress(
          GetModuleHandle(L"kernel32.dll"),
          "GetThreadPreferredUILanguages"));

 done:
  return error;
}

template<class T>
DWORD HttpDownloadOperation<T>::Start() {
  DWORD error = ERROR_SUCCESS;
  CString message;

  HRESULT hr = file_.Create(NULL, GENERIC_WRITE);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    _Module.Log(CROS_LOG_ERROR, L"Failed to create download file (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  session_ = WinHttpOpen(0,
                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME,
                         WINHTTP_NO_PROXY_BYPASS,
                         WINHTTP_FLAG_ASYNC);
  if (!session_) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to open session object (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  connection_ = WinHttpConnect(session_,
                               url_components_.lpszHostName,
                               url_components_.nPort,
                               0);
  if (!connection_) {
    error = GetLastError();
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_CONNECT,
                  url_components_.lpszHostName,
                  url_components_.nPort,
                  error);
    goto done;
  }

  DWORD flags = 0;
  if (url_components_.nScheme == INTERNET_SCHEME_HTTPS)
    flags = WINHTTP_FLAG_SECURE;
  request_ = WinHttpOpenRequest(connection_,
                                L"GET",
                                url_components_.lpszUrlPath,
                                NULL,
                                WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                flags);
  if (!request_) {
    error = GetLastError();
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_GET,
                  url_components_.lpszUrlPath,
                  error);
    goto done;
  }

  WINHTTP_STATUS_CALLBACK callback;
  callback = WinHttpSetStatusCallback(request_,
                                      WinHttpCallback,
                                      WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                                      NULL);
  if (callback == WINHTTP_INVALID_STATUS_CALLBACK) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to set HTTP callback (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  BOOL success = WinHttpSendRequest(request_,
                                    WINHTTP_NO_ADDITIONAL_HEADERS,
                                    0,
                                    WINHTTP_NO_REQUEST_DATA,
                                    0,
                                    0,
                                    reinterpret_cast<DWORD_PTR>(this));
  if (!success) {
    error = GetLastError();
    FmtMessageBox(dlg_->m_hWnd,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_ERROR_FAILED_SEND_REQUEST,
                  error);
    goto done;
  }

  error = ERROR_SUCCESS;

  // The HTTP request is performed asynchronously.
  // Events are processed in WinHttpCallback.

 done:
  dlg_->set_last_dlg_error(error);
  return error;
}

template<class T>
void HttpDownloadOperation<T>::Cancel() {
  EndAsyncHttpOps();
}

template<class T>
CString HttpDownloadOperation<T>::GetDownloadedFile() {
  CString file_name = GetTempFileName();
  file_.Close(file_name);
  return file_name;
}

// Private methods

template<class T>
BOOL HttpDownloadOperation<T>::SaveThreadPreferredUILanguages(
    __out WCHAR** languages,
    __out ULONG* num_languages) {
  DWORD error = ERROR_SUCCESS;

  if (!get_thread_preferred_ui_languages_)
    goto done;

  ULONG languages_size = 0;
  BOOL success = get_thread_preferred_ui_languages_(
                     MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES,
                     num_languages,
                     NULL,
                     &languages_size);
  if (!success) {
    error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER) {
      _Module.Log(CROS_LOG_ERROR,
                  L"Failed to query the thread preferred UI languages "
                  L"buffer size (%d)",
                  error);
      DisplayInternalErrorMessage();
      goto done;
    }
  }

  *languages = static_cast<WCHAR*>(LocalAlloc(LPTR,
                                              languages_size * sizeof(WCHAR)));
  if (!*languages) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate thread preferred UI languages buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  success = get_thread_preferred_ui_languages_(
                MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES,
                num_languages,
                *languages,
                &languages_size);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to allocate thread preferred UI languages buffer (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  return (error == ERROR_SUCCESS);
}

template<class T>
void HttpDownloadOperation<T>::RestoreThreadPreferredUILanguages(
    __in WCHAR* languages,
    __in ULONG num_languages) {
  if (!set_thread_preferred_ui_languages_ || !languages)
    return;
  set_thread_preferred_ui_languages_(MUI_LANGUAGE_NAME,
                                     languages,
                                     &num_languages);
  LocalFree(languages);
}

template<class T>
void CALLBACK HttpDownloadOperation<T>::WinHttpCallback(
    __in HINTERNET handle,
    __in DWORD_PTR context,
    __in DWORD code,
    __in void* info,
    __in DWORD length) {
  HttpDownloadOperation* op =
      reinterpret_cast<HttpDownloadOperation*>(context);
  T* dlg = op->dlg_;
  CString server = op->url_components_.lpszHostName;
  WINHTTP_ASYNC_RESULT* result;

  WCHAR* languages = NULL;
  ULONG num_languages = 0;
  if (!op->SaveThreadPreferredUILanguages(&languages, &num_languages))
    return;
  SetThreadUILanguage(_Module.ui_langid());

  switch(code) {
  case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
    _Module.Log(CROS_LOG_INFO, L"Resolving name %s", server);
    dlg->set_status(IDS_STATUS_RESOLVING_NAME, server);
    break;
  case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
    _Module.Log(CROS_LOG_INFO, L"Name %s resolved", server);
    dlg->set_status(IDS_STATUS_NAME_RESOLVED, server);
    break;
  case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
    _Module.Log(CROS_LOG_INFO, L"Connecting to server %s", server);
    dlg->set_status(IDS_STATUS_CONNECTING_TO_SERVER, server);
    break;
  case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
    _Module.Log(CROS_LOG_INFO, L"Connected to server %s", server);
    dlg->set_status(IDS_STATUS_CONNECTED_TO_SERVER, server);
    break;
  case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
    _Module.Log(CROS_LOG_INFO, L"Closing connection to server %s", server);
    dlg->set_status(IDS_STATUS_CLOSING_CONNECTION, server);
    break;
  case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
    _Module.Log(CROS_LOG_INFO, L"Connection to server %s closed", server);
    dlg->set_status(IDS_STATUS_CONNECTION_CLOSED, server);
    break;
  case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
    _Module.Log(CROS_LOG_INFO, L"Sending request");
    dlg->set_status(IDS_STATUS_SENDING_REQUEST);
    break;
  case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
    _Module.Log(CROS_LOG_INFO, L"Request sent");
    dlg->set_status(IDS_STATUS_REQUEST_SENT);
    break;
  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
    // We get here when the initial GET request sent using
    // WinHttpSendRequest completes
    _Module.Log(CROS_LOG_INFO, L"Send request complete");
    op->OnSendRequestComplete(handle);
    break;
  case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
    break;
  case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
    break;
  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    // We get here after WinHttpReceiveResponse receives headers
    _Module.Log(CROS_LOG_INFO, L"Headers available");
    op->OnHeadersAvailable(handle);
    break;
  case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
    result = (WINHTTP_ASYNC_RESULT*) info;
    _Module.Log(CROS_LOG_INFO,
                L"Request error (result=%p, error=%d)",
                result->dwResult,
                result->dwError);
    op->OnStatusRequestError(result);
    break;
  case WINHTTP_CALLBACK_STATUS_REDIRECT:
    // TODO(thieule): Handle redirect
    _Module.Log(CROS_LOG_INFO, L"Received redirect");
    dlg->set_status(IDS_STATUS_REDIRECT);
    break;
  case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
    op->OnReadComplete(handle, length);
    break;
  case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
    _Module.Log(CROS_LOG_INFO, L"Closing handle");
    dlg->PostMessage(op->done_msg_);
  default:
    break;
  }

  op->RestoreThreadPreferredUILanguages(languages, num_languages);
}

template<class T>
void HttpDownloadOperation<T>::OnSendRequestComplete(__in HINTERNET request) {
  dlg_->set_status(IDS_STATUS_SENDREQUEST_COMPLETE);
  BOOL success = WinHttpReceiveResponse(request, NULL);
  if (!success) {
    dlg_->set_last_dlg_error(GetLastError());
    EndAsyncHttpOps();
  }
}

template<class T>
void HttpDownloadOperation<T>::OnHeadersAvailable(__in HINTERNET request) {
  DWORD error;
  DWORD status_code = 0;
  DWORD size;

  size = sizeof(status_code);
  BOOL success = WinHttpQueryHeaders(request,
                                     WINHTTP_QUERY_STATUS_CODE |
                                     WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX,
                                     &status_code,
                                     &size,
                                     WINHTTP_NO_HEADER_INDEX);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to query HTTP header status (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }
  if (status_code != HTTP_STATUS_OK) {
    error = status_code;
    goto done;
  }

  size = sizeof(total_download_size_);
  success = WinHttpQueryHeaders(request,
                                WINHTTP_QUERY_CONTENT_LENGTH |
                                WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &total_download_size_,
                                &size,
                                WINHTTP_NO_HEADER_INDEX);
  if (!success) {
    error = status_code;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to query HTTP header download size (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  success = WinHttpReadData(request, buffer_, buffer_size_, NULL);
  if (!success) {
    error = status_code;
    goto done;
  }

  error = ERROR_SUCCESS;

 done:
  dlg_->set_last_dlg_error(error);
  if (error != ERROR_SUCCESS)
    EndAsyncHttpOps();
}

template<class T>
void HttpDownloadOperation<T>::OnStatusRequestError(
    __in WINHTTP_ASYNC_RESULT* result) {
  dlg_->set_status(IDS_STATUS_REQUEST_ERROR);
  dlg_->set_last_dlg_error(result->dwError);
}

template<class T>
void HttpDownloadOperation<T>::OnReadComplete(__in HINTERNET request,
                                              __in DWORD length) {
  HRESULT hr = file_.Write(buffer_, length);
  if(FAILED(hr)) {
    dlg_->set_last_dlg_error(hr);
    EndAsyncHttpOps();
    return;
  }

  curr_download_size_ += length;
  int progress = CalcProgress(curr_download_size_, total_download_size_);
  dlg_->set_progress(progress);

  CString curr_download_str = BytesToStr(curr_download_size_);
  CString total_download_str = BytesToStr(total_download_size_);
  dlg_->set_status(IDS_STATUS_DOWNLOADING,
                   curr_download_str,
                   total_download_str);

  if (curr_download_size_ < total_download_size_) {
    BOOL success = WinHttpReadData(request, buffer_, buffer_size_, NULL);
    if (!success) {
      dlg_->set_last_dlg_error(GetLastError());
      EndAsyncHttpOps();
    }
  } else {
    dlg_->set_progress(progress_end_);
    dlg_->set_status(IDS_STATUS_DOWNLOAD_COMPLETE);
    dlg_->set_last_dlg_error(ERROR_SUCCESS);
    EndAsyncHttpOps();
  }
}

template<class T>
void HttpDownloadOperation<T>::EndAsyncHttpOps() {
  if (request_ != NULL) {
    WinHttpCloseHandle(request_);
    request_ = NULL;
  }
}

#endif  // CHROMEOSRECOVERY_HTTPDOWNLOADOPERATION_H_
