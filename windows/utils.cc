// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "resource.h"
#include "utils.h"

DWORD ErrnoToWin32(__in int errno_) {
  DWORD error;
  switch(errno_)
  {
    case EACCES:
      error = ERROR_ACCESS_DENIED;
      break;
    case ENOENT:
      error = ERROR_FILE_NOT_FOUND;
      break;
    case ENOSPC:
      error = ERROR_DISK_FULL;
      break;
    default:
      error = ERROR_GEN_FAILURE;
      break;
  }

  return error;
}

DWORD ListViewInsertColumn(__in CListViewCtrl* list_ctrl,
                           __in int col,
                           __in UINT column_string_id,
                           __in int format,
                           __in int width) {
  DWORD error;

  CString column_name;
  BOOL success = column_name.LoadString(column_string_id);
  if (!success) {
    error = GetLastError();
    goto done;
  }

  int result = list_ctrl->InsertColumn(col,
                                       column_name,
                                       format,
                                       width);
  if (result == -1) {
    error = GetLastError();
    goto done;
  }

  error = ERROR_SUCCESS;

 done:
  return error;
}

#define ONE_KILOBYTE 1000
#define ONE_MEGABYTE (1000 * 1000)
#define ONE_GIGABYTE (1000 * 1000 * 1000)

CString BytesToStr(__in __int64 bytes) {
  CString str;
  float temp;
  if (bytes < 65536) {
    str.Format(L"%d B", bytes);
  } else if (bytes < ONE_MEGABYTE) {
    temp = static_cast<float>(bytes) / static_cast<float>(ONE_KILOBYTE);
    str.Format(L"%lu KB", static_cast<ULONG>(temp));
  } else if (bytes < ONE_GIGABYTE) {
    temp = static_cast<float>(bytes) / static_cast<float>(ONE_MEGABYTE);
    str.Format(L"%lu MB", static_cast<ULONG>(temp));
  } else {
    temp = static_cast<float>(bytes) / static_cast<float>(ONE_GIGABYTE);
    str.Format(L"%.2f GB", temp);
  }

  return str;
}

int FmtMessageBox(__in HWND parent, __in UINT type, __in UINT format_id, ...) {
  CString format;
  format.LoadString(format_id);

  CString message;
  va_list args;
  va_start(args, format_id);
  message.FormatV(format, args);
  va_end(args);

  if (type & CROS_MB_INCLUDE_LOG_INFO) {
    CString log_file;
    log_file.Format(IDS_LOG_FILE_LOCATION_FORMAT, _Module.log_file_name());
    message += L"\r\n\r\n";
    message += log_file;
    type &= ~CROS_MB_INCLUDE_LOG_INFO;
  }

  CString caption;
  caption.LoadString(IDS_CHROMEOSRECOVERY);

  return ::MessageBox(parent, message, caption, type);
}

void DisplayInternalErrorMessage() {
  FmtMessageBox(NULL,
                MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                IDS_ERROR_INTERNAL);
}

CString GetTempFileName() {
  CString name;

  WCHAR temp_dir[MAX_PATH + 1] = L"";
  DWORD ret = GetTempPath(RTL_NUMBER_OF(temp_dir), temp_dir);
  if (ret == 0)
    goto done;

  WCHAR temp_name[MAX_PATH + 1] = L"";
  ret = GetTempFileName(temp_dir, L"Cros", 0, temp_name);
  if (ret == 0)
    goto done;

  name = temp_name;

 done:
  return name;
}

DWORD GetFileSize(__in const CString& file_name, __in ULONGLONG* file_size) {
  DWORD error = ERROR_SUCCESS;

  CAtlFile file;
  HRESULT hr = file.Create(file_name,
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           OPEN_EXISTING,
                           0,
                           NULL,
                           NULL);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    goto done;
  }

  hr = file.GetSize(*file_size);
  if (FAILED(hr)) {
    error = (DWORD) hr;
    goto done;
  }
  file.Close();

 done:
  return error;
}

DWORD DismountUsbDiskVolumes(__in DWORD usb_disk_index,
                             __out HANDLES* volume_handles) {
  DWORD error;
  HRESULT hr;
  BOOL success;
  HANDLE finder;
  BOOL more = TRUE;
  WCHAR volume_path[MAX_PATH + 1];

  volume_handles->clear();

  for(finder = FindFirstVolume(volume_path, RTL_NUMBER_OF(volume_path));
      finder != INVALID_HANDLE_VALUE && more;
      more = FindNextVolume(finder, volume_path, RTL_NUMBER_OF(volume_path))) {
        size_t length;
        hr = StringCchLength(volume_path, RTL_NUMBER_OF(volume_path), &length);
        if (FAILED(hr)) {
          error = (DWORD) hr;
          goto done;
        }

        CAtlFile volume;
        volume_path[length - 1] = L'\0';
        hr = volume.Create(volume_path,
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           OPEN_EXISTING,
                           0,
                           NULL,
                           NULL);
        if (FAILED(hr)) {
          error = (DWORD) hr;
          _Module.Log(CROS_LOG_ERROR,
                      L"Failed to open volume %s for dismounting (%d)",
                      volume_path,
                      error);
          goto done;
        }

        VOLUME_DISK_EXTENTS disk_extents;
        DWORD bytes_returned;
        success = DeviceIoControl(volume,
                                  IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                  NULL,
                                  0,
                                  &disk_extents,
                                  sizeof(disk_extents),
                                  &bytes_returned,
                                  NULL);
        if (!success) {
          error = GetLastError();
          if (error == ERROR_MORE_DATA ||
              error == ERROR_INVALID_FUNCTION ||
              error == ERROR_NOT_READY) {
            _Module.Log(CROS_LOG_ERROR,
                        L"Failed to query volume %s disk extents, "
                        L"skipping (%d)",
                        volume_path,
                        error);
            continue;
          }
          _Module.Log(CROS_LOG_ERROR,
                      L"Failed to query volume %s disk extents (%d)",
                      volume_path,
                      error);
          goto done;
        }

        if (disk_extents.NumberOfDiskExtents != 1 ||
            disk_extents.Extents[0].DiskNumber != usb_disk_index) {
          continue;
        }

        success = DeviceIoControl(volume,
                                  FSCTL_LOCK_VOLUME,
                                  NULL,
                                  0,
                                  NULL,
                                  0,
                                  &bytes_returned,
                                  NULL);
        if (!success) {
          error = GetLastError();
          _Module.Log(CROS_LOG_WARN,
                      L"Failed to lock volume %s, forcing dismount (%d)",
                      volume_path,
                      error);
        }

        success = DeviceIoControl(volume,
                                  FSCTL_DISMOUNT_VOLUME,
                                  NULL,
                                  0,
                                  NULL,
                                  0,
                                  &bytes_returned,
                                  NULL);
        if (!success) {
          error = GetLastError();
          _Module.Log(CROS_LOG_ERROR,
                      L"Failed to dismount volume %s (%d)",
                      volume_path,
                      error);
          goto done;
        }

        volume_handles->push_back(volume.Detach());
  }

  error = ERROR_SUCCESS;

 done:
  if (finder != INVALID_HANDLE_VALUE)
    FindVolumeClose(finder);
  if (error != ERROR_SUCCESS) {
    HANDLES::iterator it;
    for (it = volume_handles->begin(); it != volume_handles->end(); ++it)
      CloseHandle(*it);
    volume_handles->clear();
  }

  return error;
}

DWORD EjectUsbDisk(DWORD usb_disk_index) {
  DWORD error = ERROR_SUCCESS;
  CHandle disk;

  WCHAR usb_dasd[MAX_PATH + 1];
  StringCchPrintf(usb_dasd,
                  RTL_NUMBER_OF(usb_dasd),
                  L"\\\\.\\PhysicalDrive%d",
                  usb_disk_index);
  HANDLE temp = CreateFile(usb_dasd,
                           GENERIC_READ,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                           NULL);
  if (temp == INVALID_HANDLE_VALUE) {
    error = GetLastError();
    goto done;
  }
  disk.Attach(temp);

  DWORD bytes_returned;
  BOOL success = DeviceIoControl(disk,
                                 IOCTL_STORAGE_EJECT_MEDIA,
                                 NULL,
                                 0,
                                 NULL,
                                 0,
                                 &bytes_returned,
                                 NULL);
  if (!success) {
    error = GetLastError();
    goto done;
  }

 done:
  return error;
}
