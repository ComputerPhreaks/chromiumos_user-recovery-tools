// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_SELECT_DEVICE_PAGE_
#define CHROMEOSRECOVERY_SELECT_DEVICE_PAGE_

#include "resource.h"

class ChromeOSRecoveryWizard;
class Device;

// Wizard page that allows the user to select a Chrome OS device for which
// he wants to create the recovery media.
class SelectDevicePage : public CPropertyPageImpl<SelectDevicePage>,
                         public CWinDataExchange<SelectDevicePage> {
 public:
  enum { IDD = IDD_PROPPAGE_SELECT_DEVICE };

  explicit SelectDevicePage(__in ChromeOSRecoveryWizard* parent);
  virtual ~SelectDevicePage();

  // Returns the selected Chrome OS recovery device.
  Device* GetSelectedDevice();

  // Returns the name of the selected Chrome OS recovery device.
  CString GetSelectedDeviceName();

  BEGIN_MSG_MAP(SelectDevicePage)
    MSG_WM_INITDIALOG(OnInitDialog)
    NOTIFY_HANDLER_EX(IDC_DEVICES_LIST,	NM_CLICK, OnDevicesListClick)
    NOTIFY_HANDLER_EX(IDC_DEVICES_LIST, LVN_GETINFOTIP, OnGetInfoTip)
    NOTIFY_CODE_HANDLER_EX(CROS_PSN_LOCAL_IMAGE, OnLocalImage)
    COMMAND_ID_HANDLER(IDC_LANGUAGES, OnLanguageChange)
    CHAIN_MSG_MAP(CPropertyPageImpl<SelectDevicePage>)
  END_MSG_MAP()

  BEGIN_DDX_MAP(SelectDevicePage)
    DDX_CONTROL_HANDLE(IDC_LANGUAGES, languages_combo_box_);
    DDX_CONTROL_HANDLE(IDC_DEVICES_LIST, devices_list_ctrl_);
  END_DDX_MAP()

  // WTL calls this method to initialize the page.
  BOOL OnInitDialog(__in HWND focus, __in LPARAM param);

  // WTL calls this method when the user on the devices list control.
  LRESULT OnDevicesListClick(__in LPNMHDR nmh);

  // WTL calls this method to retrieve the tool tip for an item in the devices
  // list control.
  LRESULT OnGetInfoTip(__in LPNMHDR nmh);

  // WTL calls this method when the user clicks on the Local Image button.
  LRESULT OnLocalImage(__in LPNMHDR nmh);

  // WTL calls this method when the user changes the language.
  LRESULT OnLanguageChange(__in WORD code,
                           __in WORD id,
                           __in HWND ctrl,
                           __inout BOOL& handled);

  // WTL calls this method when the page is set as active.
  int OnSetActive();

 private:
  // EnumSystemLocales() callback for each installed language in the system.
  static BOOL CALLBACK EnumSystemLocalesCallback(__in LPTSTR locale_name);

  // Initializes the languages combo box.
  void InitLanguagesComboBox();

  // Initializes the devices list control.
  void InitDevicesListCtrl();

  // Inserts Chrome OS recovery devices into the devices list control.
  void InsertDevicesIntoListCtrl();

  // Updates the states of all controls on this page.
  void UpdateDialogControls();

  // Indices of columns in the devices list control.
  static int kDevicesListNameColumn;
  static int kDevicesListDescColumn;

  // Pointer to the wizard that hosts this page.
  ChromeOSRecoveryWizard* parent_;

  // Combo box containing the various languages that we support.
  CComboBox languages_combo_box_;

  // List control containing Chrome OS devices that the user can select from.
  CListViewCtrl devices_list_ctrl_;

  DISALLOW_COPY_AND_ASSIGN(SelectDevicePage);
};

#endif  // CHROMEOSRECOVERY_SELECT_DEVICE_PAGE_
