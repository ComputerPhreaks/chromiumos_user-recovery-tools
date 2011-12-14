// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_UTILS_H_
#define CHROMEOSRECOVERY_UTILS_H_

#define CROS_MB_INCLUDE_LOG_INFO  0x10000000

typedef std::list<std::wstring> STRING_LIST;

// Converts a CRT errno to a Win32 error code.
DWORD ErrnoToWin32(__in int errno_);

// Inserts a column into a list control.
DWORD ListViewInsertColumn(__in CListViewCtrl* list_ctrl,
                           __in int col,
                           __in UINT column_string_id,
                           __in int format,
                           __in int width);

// Autosizes all columns of a list control to their contents.
void AutoSizeColumns(__in CListViewCtrl* list_ctrl);

// Convert the number of bytes into a string with the proper unit (MB, GB, etc.)
CString BytesToStr(__in __int64 bytes);

// Displays a message box.
int FmtMessageBox(__in HWND parent, __in UINT type, __in UINT format_id, ...);

// Displays an internal error message box and include the full path to the
// log file.
void DisplayInternalErrorMessage();

// Returns a temporary file name.
CString GetTempFileName();

// Returns the size, in bytes, of the specified file.
DWORD GetFileSize(__in const CString& file_name, __in ULONGLONG* file_size);

// Dismounts all volumes on the specified disk.
// Handles to the dismounted volumes are returned in |volume_handles|. Do not
// close these handles until all operations on the disk and volumes are
// complete.  This prevents other processes from accessing the volumes.
typedef std::vector<HANDLE> HANDLES;
DWORD DismountUsbDiskVolumes(__in DWORD usb_disk_index,
                             __out HANDLES* volume_handles);

// Ejects the specified disk.
DWORD EjectUsbDisk(DWORD usb_disk_index);

#endif  // CHROMEOSRECOVERY_UTILS_H_
