// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "select_usb_page.h"
#include "utils.h"

int SelectUSBPage::kUsbListNameColumn = 0;
int SelectUSBPage::kUsbListSizeColumn = 1;
float SelectUSBPage::kMinDeviceSizeInGb = 3.5;

// SelectUSBPage dialog

SelectUSBPage::SelectUSBPage(__in ChromeOSRecoveryWizard* parent)
    : CPropertyPageImpl<SelectUSBPage>(IDS_CHROMEOSRECOVERY),
      parent_(parent) {
  selected_usb_index_ = -1;
  selected_usb_disk_index_ = -1;
}

// Message handlers

BOOL SelectUSBPage::OnInitDialog(__in HWND focus, __in LPARAM param) {
  DoDataExchange();
  warning_icon_.SetIcon(LoadIcon(NULL, IDI_WARNING));
  InitUsbListCtrl();
  return TRUE;
}

LRESULT SelectUSBPage::OnUsbListItemChanged(__in LPNMHDR nmh) {
  selected_usb_index_ = usb_list_ctrl_.GetSelectedIndex();
  if (selected_usb_index_ == -1) {
    selected_usb_disk_index_ = -1;
  } else {
    selected_usb_disk_index_ = usb_list_ctrl_.GetItemData(selected_usb_index_);
  }

  UpdateDialogControls();

  return 0;
}

LRESULT SelectUSBPage::OnDeviceChange(__in UINT msg,
                                      __in WPARAM wparam,
                                      __in LPARAM lparam,
                                      __inout BOOL& handled) {
  if (wparam != DBT_DEVICEARRIVAL && wparam != DBT_DEVICEREMOVECOMPLETE)
    goto done;

  // We need to handle this message as quickly as possible to prevent hanging
  // PnP, so queue up a custom message to ourself to do the actual refresh
  PostMessage(WM_SELECTUSBPAGE_REFRESH_DEVICE);

 done:
  return 0;
}

LRESULT SelectUSBPage::OnRefreshUsbDevices(__in UINT msg,
                                           __in WPARAM wparam,
                                           __in LPARAM lparam,
                                           __inout BOOL& handled) {
  PopulateUsbListCtrl();
  return 0;
}

// Notifications

int SelectUSBPage::OnSetActive() {
  UpdateDialogControls();
  PopulateUsbListCtrl();
  return 0;
}

int SelectUSBPage::OnWizardNext() {
  Device* device = parent_->select_device_page()->GetSelectedDevice();
  if (!device) {
    parent_->SetActivePage(ChromeOSRecoveryWizard::kWriteImagePageIndex);
    return -1;
  } else {
    return 0;
  }
}

// Accessors and mutators

DWORD SelectUSBPage::selected_usb_disk_index() {
  return selected_usb_disk_index_;
}

// Private methods

void SelectUSBPage::InitUsbListCtrl() {
  ListViewInsertColumn(&usb_list_ctrl_,
                       kUsbListNameColumn,
                       IDS_USB_LIST_COLUMN_NAME,
                       LVCFMT_LEFT,
                       450);

  ListViewInsertColumn(&usb_list_ctrl_,
                       kUsbListSizeColumn,
                       IDS_USB_LIST_COLUMN_SIZE,
                       LVCFMT_LEFT,
                       100);

  usb_list_ctrl_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT,
                                          LVS_EX_FULLROWSELECT);
}

DWORD SelectUSBPage::PopulateUsbListCtrl() {
  HRESULT hr;
  CComPtr<IWbemLocator> loc;
  CComPtr<IWbemServices> services;
  CComPtr<IEnumWbemClassObject> enum_disks;
  CWaitCursor wait_cursor;

  usb_list_ctrl_.DeleteAllItems();

  hr = CoCreateInstance(CLSID_WbemLocator,
                        0,
                        CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator,
                        (LPVOID*) &loc);
  if (FAILED(hr)) {
    _Module.Log(CROS_LOG_ERROR, L"Failed to create Wbem locator (%d)", hr);
    goto done;
  }

  hr = loc->ConnectServer(CComBSTR(L"root\\cimv2"),
                          NULL,
                          NULL,
                          0,
                          NULL,
                          0,
                          0,
                          &services);
  if (FAILED(hr)) {
    _Module.Log(CROS_LOG_ERROR, L"Failed to connect to root\\cimv2 (%d)", hr);
    goto done;
  }

  hr = services->CreateInstanceEnum(CComBSTR(L"Win32_DiskDrive"),
                                    WBEM_FLAG_FORWARD_ONLY,
                                    NULL,
                                    &enum_disks);
  if (FAILED(hr)) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to enumerate Win32_DiskDrive (%d)",
                hr);
    goto done;
  }

  usb_list_ctrl_.DeleteAllItems();

  int index = -1;
  while (TRUE) {
    CComPtr<IWbemClassObject> disk;
    ULONG num_returned;
    hr = enum_disks->Next(WBEM_INFINITE, 1, &disk, &num_returned);
    if (hr != WBEM_S_NO_ERROR)
      break;

    CComVariant caption;
    hr = disk->Get(L"Caption", 0, &caption, NULL, NULL);
    if (FAILED(hr)) {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to retrieve disk Caption, skipping (%d)",
                  hr);
      continue;
    }

    _Module.Log(CROS_LOG_INFO, L"Found disk '%s'", caption.bstrVal);

    CComVariant interface_type;
    hr = disk->Get(L"InterfaceType", 0, &interface_type, NULL, NULL);
    if (FAILED(hr)) {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to retrieve disk '%s' InterfaceType, skipping (%d)",
                  caption.bstrVal,
                  hr);
      continue;
    }

    CComVariant media_type;
    hr = disk->Get(L"MediaType", 0, &media_type, NULL, NULL);
    if (FAILED(hr)) {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to retrieve disk '%s' MediaType, skipping (%d)",
                  caption.bstrVal,
                  hr);
      continue;
    }

    CString media_type_str(media_type.bstrVal);
    media_type_str.MakeLower();
    if (_wcsicmp(interface_type.bstrVal, L"USB") != 0 ||
        media_type_str.Find(L"removable media") == -1) {
      _Module.Log(CROS_LOG_WARN,
                  L"Skipping disk '%s' with InterfaceType='%s' MediaType='%s'",
                  caption.bstrVal,
                  interface_type.bstrVal,
                  media_type.bstrVal);
      continue;
    }

    CComVariant size;
    hr = disk->Get(L"Size", 0, &size, NULL, NULL);
    if (FAILED(hr)) {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to retrieve disk '%s' Size, skipping (%d)",
                  caption.bstrVal,
                  hr);
      continue;
    }

    CComVariant disk_index;
    hr = disk->Get(L"Index", 0, &disk_index, NULL, NULL);
    if (FAILED(hr)) {
      _Module.Log(CROS_LOG_WARN,
                  L"Failed to retrieve disk '%s' Index, skipping (%d)",
                  caption.bstrVal,
                  hr);
      continue;
    }

    CString size_str = BytesToStr(_wtoi64(size.bstrVal));

    _Module.Log(CROS_LOG_INFO, L"Adding disk '%s'", caption.bstrVal);
    index = usb_list_ctrl_.AddItem(++index,
                                   kUsbListNameColumn,
                                   caption.bstrVal);
    usb_list_ctrl_.SetItemText(index, kUsbListSizeColumn, size_str);
    usb_list_ctrl_.SetItemData(index, (DWORD_PTR) disk_index.intVal);

    if (disk_index.intVal == selected_usb_disk_index_) {
      usb_list_ctrl_.SelectItem(selected_usb_index_);
    }
  }

  int num_drives = usb_list_ctrl_.GetItemCount();
  if (!num_drives) {
    CString insert_usb_message;
    insert_usb_message.Format(IDS_USB_LIST_INSERT_DISK);
    index = usb_list_ctrl_.AddItem(0,
                                   kUsbListNameColumn,
                                   insert_usb_message);
    usb_list_ctrl_.SetItemData(index, -1);
  } else if (num_drives == 1 && !usb_list_ctrl_.GetSelectedCount()) {
    usb_list_ctrl_.SelectItem(0);
  }
  
  usb_list_ctrl_.SetColumnWidth(kUsbListNameColumn, LVSCW_AUTOSIZE_USEHEADER);
  usb_list_ctrl_.SetColumnWidth(kUsbListSizeColumn, LVSCW_AUTOSIZE_USEHEADER);

  UpdateDialogControls();

  hr = S_OK;

 done:
  if (FAILED(hr))
    DisplayInternalErrorMessage();

  return (DWORD) hr;
}

void SelectUSBPage::UpdateDialogControls() {
  if (selected_usb_index_ != -1) {
    DWORD_PTR data = usb_list_ctrl_.GetItemData(selected_usb_index_);
    if (data != -1) {
      CString usb_name;
      usb_list_ctrl_.GetItemText(selected_usb_index_,
                                 kUsbListNameColumn,
                                 usb_name);

      warning_icon_.ShowWindow(SW_SHOW);

      CString size_str;
      usb_list_ctrl_.GetItemText(selected_usb_index_,
                                 kUsbListSizeColumn,
                                 size_str);
      float size = static_cast<float>(_wtof(size_str));
      if (size < kMinDeviceSizeInGb) {
        warning_text_.Format(IDS_DEVICE_TOO_SMALL_WARNING_FORMAT, size_str);
        parent_->SetWizardButtons(PSWIZB_BACK);
        goto done;
      }

      warning_text_.Format(IDS_ERASE_CONFIRMATION_FORMAT, usb_name);
      parent_->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
      goto done;
    }
  }

  warning_icon_.ShowWindow(SW_HIDE);
  warning_text_ = L"";
  parent_->SetWizardButtons(PSWIZB_BACK);

 done:
  DoDataExchange();
}
