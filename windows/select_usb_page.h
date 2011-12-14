// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_SELECT_USB_PAGE_
#define CHROMEOSRECOVERY_SELECT_USB_PAGE_

#include "resource.h"
#include "utils.h"

#define WM_SELECTUSBPAGE_REFRESH_DEVICE  (WM_USER + 789)

class ChromeOSRecoveryWizard;

// Wizard page that allows the user to select the USB device that will be
// used as the recovery media.
class SelectUSBPage : public CPropertyPageImpl<SelectUSBPage>,
                      public CWinDataExchange<SelectUSBPage> {
 public:
  enum { IDD = IDD_PROPPAGE_SELECT_USB };

  explicit SelectUSBPage(__in ChromeOSRecoveryWizard* parent);

  BEGIN_MSG_MAP(SelectUSBPage)
    MSG_WM_INITDIALOG(OnInitDialog)
    NOTIFY_HANDLER_EX(IDC_USB_LIST, LVN_ITEMCHANGED, OnUsbListItemChanged)
    MESSAGE_HANDLER(WM_DEVICECHANGE, OnDeviceChange)
    MESSAGE_HANDLER(WM_SELECTUSBPAGE_REFRESH_DEVICE, OnRefreshUsbDevices)
    CHAIN_MSG_MAP(CPropertyPageImpl<SelectUSBPage>)
  END_MSG_MAP()

  BEGIN_DDX_MAP(SelectUSBPage)
    DDX_CONTROL_HANDLE(IDC_USB_LIST, usb_list_ctrl_);
    DDX_CONTROL_HANDLE(IDC_WARNING_ICON, warning_icon_);
    DDX_TEXT(IDC_WARNING_TEXT, warning_text_);
  END_DDX_MAP()

  // WTL calls this method to initialize the page.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this method when the user selects a different removable media.
  LRESULT OnUsbListItemChanged(__in LPNMHDR nmh);

  // WTL calls this method when a device has been inserted or removed from
  // the operating system.
  LRESULT OnDeviceChange(__in UINT msg,
                         __in WPARAM wparam,
                         __in LPARAM lparam,
                         __inout BOOL& handled);

  // Since WM_DEVICECHANGE blocks PnP, we do not want to do any processing in
  // OnDeviceChange().  Instead, we post this message back to ourselves to
  // refresh the USB devices list.
  LRESULT OnRefreshUsbDevices(__in UINT msg,
                              __in WPARAM wparam,
                              __in LPARAM lparam,
                              __inout BOOL& handled);

  // WTL calls this method when the page is set active.
  int OnSetActive();

  // WTL calls this method when the user clicks on the Next button.
  int OnWizardNext();

  DWORD selected_usb_disk_index();

 private:
  // Initializes the USB device list control.
  void InitUsbListCtrl();

  // Populates the USB device list control with all removable USB media in
  // the system.
  DWORD PopulateUsbListCtrl();

  // Updates the state of all controls in this page.
  void UpdateDialogControls();

  // Indices of the columns in the USB device list control.
  static int kUsbListNameColumn;
  static int kUsbListSizeColumn;

  // Minimum size, in GB, of the removable media that we can use for recovery.
  static float kMinDeviceSizeInGb;

  // Pointer to the parent wizard that hosts this page.
  ChromeOSRecoveryWizard* parent_;

  // List control that contains list of USB devices that the user can select
  // to be used as the recovery media.
  CListViewCtrl usb_list_ctrl_;

  // Warning icon and warning text that is displayed to tell the user that the
  // selected device will be erased.
  CStatic warning_icon_;
  CString warning_text_;

  // List control index of the selected USB device.
  int selected_usb_index_;

  // Physical disk index of the select USB device.
  DWORD selected_usb_disk_index_;

  DISALLOW_COPY_AND_ASSIGN(SelectUSBPage);
};

#endif  // CHROMEOSRECOVERY_SELECT_USB_PAGE_
