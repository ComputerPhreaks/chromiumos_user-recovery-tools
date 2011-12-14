// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_PROGRESSDLG_H_
#define CHROMEOSRECOVERY_PROGRESSDLG_H_

#include "resource.h"

// Base progress dialog class.
template<class T>
class ProgressDlg {
 public:
  ProgressDlg();

  // WTL calls this method to initialize the page.
  virtual BOOL OnInitDialog(__in HWND focus, __in LPARAM lparam);

  // WTL calls this method when the user clicks on the Cancel button.
  // Subclass should not override this method.  This method will call
  // OnCancel() to notify the subclass when the user cancels the dialog.
  void OnCancelInternal(__in UINT code, __in int id, __in HWND ctrl);

  // Subclass should implement this to handle cancellation.
  virtual void OnCancel(__in UINT code, __in int id, __in HWND ctrl);

  BOOL set_status(__in UINT format_id, ...);
  BOOL set_status(__in const CString& status);
  int set_progress(__in int progress);
  void set_last_dlg_error(__in DWORD error);
  DWORD last_dlg_error();

 protected:
  // Returns the current system time as a LARGE_INTEGER.
  LARGE_INTEGER GetSystemTimeAsLargeInteger();

  // Converts the relative system time into a string.
  CString RelativeSystemTimeToStr(__in const SYSTEMTIME& st);

  // Calculates the amount of time remaining.
  CString CalcTimeRemaining(__in int progress);

  // The progress bar control.
  CProgressBarCtrl progress_;

  // The status text control.
  CStatic status_;

  // The last error encountered by the dialog.
  DWORD last_dlg_error_;

  // This is set to TRUE if the user clicks on the Cancel button.
  BOOL cancel_requested_;

  // The starting time.  This is used to calculate remaining time.
  LARGE_INTEGER start_progress_time_;

  // String that contains "Calculating...".  We load it into a member so that
  // we don't have to continually load it from the resource.
  CString calculating_str_;

  DISALLOW_COPY_AND_ASSIGN(ProgressDlg<T>);
};

template<class T>
ProgressDlg<T>::ProgressDlg() {
  last_dlg_error_ = ERROR_SUCCESS;
  cancel_requested_ = FALSE;
  start_progress_time_.QuadPart = 0;
  calculating_str_.LoadString(IDS_TIME_REMAINING_CALCULATING);
}

// Message handlers

template<class T>
BOOL ProgressDlg<T>::OnInitDialog(__in HWND focus, __in LPARAM lparam) {
  T* t = static_cast<T*>(this);

  CString remaining;
  CString calculating;
  calculating.LoadString(IDS_TIME_REMAINING_CALCULATING);
  remaining.Format(IDS_TIME_REMAINING_FORMAT, calculating);
  t->SetDlgItemText(IDC_TIME_REMAINING, remaining);

  progress_.Attach(t->GetDlgItem(IDC_PROGRESS));
  progress_.SetRange(0, 100);

  status_.Attach(t->GetDlgItem(IDC_STATUS));

  t->CenterWindow();

  return TRUE;
}

template <class T>
void ProgressDlg<T>::OnCancelInternal(__in UINT code,
                                      __in int id,
                                      __in HWND ctrl) {
  T* t = static_cast<T*>(this);
  int ret = FmtMessageBox(t->m_hWnd,
                          MB_YESNO | MB_ICONQUESTION |
                          MB_APPLMODAL | MB_SETFOREGROUND,
                          IDS_CONFIRM_CANCEL);
  if (ret == IDYES)
    OnCancel(code, id, ctrl);
}

template<class T>
void ProgressDlg<T>::OnCancel(__in UINT code, __in int id, __in HWND ctrl) {
}

// Accessors and mutators

template<class T>
BOOL ProgressDlg<T>::set_status(__in UINT format_id, ...) {
  CString status;

  CString format;
  BOOL success = format.LoadString(format_id);
  if (!success)
    goto done;

  va_list args;
  va_start(args, format_id);
  success = status.FormatV(format, args);
  va_end(args);
  if (!success)
    goto done;

  success = status_.SetWindowText(status);

 done:
  return success;
}

template<class T>
BOOL ProgressDlg<T>::set_status(__in const CString& status) {
  return status_.SetWindowText(status);
}

template<class T>
int ProgressDlg<T>::set_progress(__in int progress) {
  CString time_remaining = CalcTimeRemaining(progress);

  int curr_pos = progress_.GetPos();
  if (curr_pos != progress) {
    CString remaining;
    remaining.Format(IDS_TIME_REMAINING_FORMAT, time_remaining);
    T* t = static_cast<T*>(this);
    t->SetDlgItemText(IDC_TIME_REMAINING, remaining);
    curr_pos = progress_.SetPos(progress);
  }

  return curr_pos;
}

template<class T>
void ProgressDlg<T>::set_last_dlg_error(__in DWORD error) {
  last_dlg_error_ = error;
}

template<class T>
DWORD ProgressDlg<T>::last_dlg_error() {
  return last_dlg_error_;
}

// Private methods

template<class T>
LARGE_INTEGER ProgressDlg<T>::GetSystemTimeAsLargeInteger() {
  FILETIME time_ft;
  LARGE_INTEGER time;
  GetSystemTimeAsFileTime(&time_ft);
  time.LowPart = time_ft.dwLowDateTime;
  time.HighPart = time_ft.dwHighDateTime;
  return time;
}

template<class T>
CString ProgressDlg<T>::RelativeSystemTimeToStr(__in const SYSTEMTIME& st) {
  CString time;
  CString day_label;
  CString hour_label;
  CString minute_label;
  CString second_label;

  if (st.wDay == 1)
    day_label.LoadString(IDS_DAY);
  else
    day_label.LoadString(IDS_DAYS);
  if (st.wHour == 1)
    hour_label.LoadString(IDS_HOUR);
  else
    hour_label.LoadString(IDS_HOURS);
  if (st.wMinute == 1)
    minute_label.LoadString(IDS_MINUTE);
  else
    minute_label.LoadString(IDS_MINUTES);
  if (st.wSecond == 1)
    second_label.LoadString(IDS_SECOND);
  else
    second_label.LoadString(IDS_SECONDS);

  if (st.wDay > 1) {
    float days = static_cast<float>(st.wDay - 1);
    days += static_cast<float>(st.wHour / 24.0);
    time.Format(L"%.1f %s", days, day_label);
  } else if (st.wHour > 0) {
    if (!st.wMinute)
      time.Format(L"%d %s", st.wHour, hour_label);
    else
      time.Format(L"%d %s, %d %s",
                  st.wHour,
                  hour_label,
                  st.wMinute,
                  minute_label);
  } else if (st.wMinute > 0) {
    if (!st.wSecond)
      time.Format(L"%d %s", st.wMinute, minute_label);
    else
      time.Format(L"%d %s, %d %s",
                  st.wMinute,
                  minute_label,
                  st.wSecond,
                  second_label);
  } else {
    time.Format(L"%d %s", st.wSecond, second_label);
  }

  return time;
}

template<class T>
CString ProgressDlg<T>::CalcTimeRemaining(__in int progress) {
  if (start_progress_time_.QuadPart == 0)
    start_progress_time_ = GetSystemTimeAsLargeInteger();

  if (progress == 0)
    return calculating_str_;

  LARGE_INTEGER curr_time;
  LARGE_INTEGER diff;
  LARGE_INTEGER time_per_percent;
  curr_time = GetSystemTimeAsLargeInteger();
  diff.QuadPart = curr_time.QuadPart - start_progress_time_.QuadPart;
  time_per_percent.QuadPart = diff.QuadPart / static_cast<LONGLONG>(progress);

  LARGE_INTEGER time_remaining;
  time_remaining.QuadPart = time_per_percent.QuadPart *
                            static_cast<LONGLONG>(100 - progress);

  FILETIME ft;
  SYSTEMTIME st;
  ft.dwLowDateTime = time_remaining.LowPart;
  ft.dwHighDateTime = time_remaining.HighPart;
  FileTimeToSystemTime(&ft, &st);
  return RelativeSystemTimeToStr(st);
}

#endif  // CHROMEOSRECOVERY_PROGRESSDLG_H_
