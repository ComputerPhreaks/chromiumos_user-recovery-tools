// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winnls.h>
#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "config_data.h"
#include "select_device_page.h"
#include "utils.h"

int SelectDevicePage::kDevicesListNameColumn = 0;
int SelectDevicePage::kDevicesListDescColumn = 1;

// SelectDevicePage dialog

SelectDevicePage::SelectDevicePage(__in ChromeOSRecoveryWizard* parent)
    : CPropertyPageImpl<SelectDevicePage>(IDS_CHROMEOSRECOVERY),
      parent_(parent) {
}

SelectDevicePage::~SelectDevicePage() {
}

Device* SelectDevicePage::GetSelectedDevice() {
  Device* device = NULL;
  int selection = devices_list_ctrl_.GetSelectedIndex();
  if (selection == -1)
    goto done;

  DWORD_PTR data = devices_list_ctrl_.GetItemData(selection);
  device = reinterpret_cast<Device*>(data);

 done:
  return device;
}

CString SelectDevicePage::GetSelectedDeviceName() {
  CString sel_device_name;

  int selection = devices_list_ctrl_.GetSelectedIndex();
  if (selection == -1)
    goto done;

  devices_list_ctrl_.GetItemText(selection,
                                  kDevicesListNameColumn,
                                  sel_device_name);

 done:
  return sel_device_name;
}

// Message handlers

BOOL SelectDevicePage::OnInitDialog(__in HWND focus, __in LPARAM param) {
  GetParent().CenterWindow();
  DoDataExchange();
  InitLanguagesComboBox();
  InitDevicesListCtrl();
  InsertDevicesIntoListCtrl();

  // Hide the languages dropdown for XP and extend the intro text static.
  if (_Module.is_win_xp_or_older()) {
    ::ShowWindow(GetDlgItem(IDC_LANGUAGES), SW_HIDE);

    CRect languages_rect;
    CWindow languages_dropdown = GetDlgItem(IDC_LANGUAGES);
    languages_dropdown.GetWindowRect(languages_rect);
    ScreenToClient(languages_rect);

    CRect intro_text_rect;
    CWindow intro_text = GetDlgItem(IDC_INTRO_TEXT);
    intro_text.GetWindowRect(intro_text_rect);
    ScreenToClient(intro_text_rect);
    intro_text_rect.right = languages_rect.right;
    intro_text.MoveWindow(intro_text_rect);
  }

  return TRUE;
}

LRESULT SelectDevicePage::OnDevicesListClick(__in LPNMHDR nmh) {
  UpdateDialogControls();
  return 0;
}

LRESULT SelectDevicePage::OnGetInfoTip(__in LPNMHDR nmh) {
  LPNMLVGETINFOTIP tip = (LPNMLVGETINFOTIP) nmh;
  DWORD_PTR data = devices_list_ctrl_.GetItemData(tip->iItem);
  Device* device = reinterpret_cast<Device*>(data);

  CString hwids;
  Device::HWID_LIST::iterator it;
  for (it = device->hwids()->begin();
       it != device->hwids()->end();
       ++it) {
    if (hwids.GetLength())
      hwids += L"\r\n";
    hwids += *it;
  }
  StringCchCopy(tip->pszText, tip->cchTextMax, hwids);

  return 0;
}

LRESULT SelectDevicePage::OnLocalImage(__in LPNMHDR nmh) {
  CFileDialog open_file_dlg(TRUE,
                            NULL,
                            NULL,
                            OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                            L"*.bin\0*.bin\0\0");
  int ret = open_file_dlg.DoModal(m_hWnd);
  if (ret != IDOK)
    goto done;

  int index = devices_list_ctrl_.AddItem(
                  devices_list_ctrl_.GetItemCount(),
                  kDevicesListNameColumn,
                  open_file_dlg.m_szFileName);
  devices_list_ctrl_.SetItemData(index, NULL);
  devices_list_ctrl_.SelectItem(index);
  UpdateDialogControls();
  parent_->SetActivePageByID(IDD_PROPPAGE_SELECT_USB);

 done:
  return 0;
}

LRESULT SelectDevicePage::OnLanguageChange(__in WORD code,
                                           __in WORD id,
                                           __in HWND ctrl,
                                           __inout BOOL& handled) {
  if (code != CBN_SELCHANGE)
    return 0;

  int selection = languages_combo_box_.GetCurSel();
  if (selection >= 0) {
    LANGID langid =
        static_cast<LANGID>(languages_combo_box_.GetItemData(selection));
    _Module.set_ui_langid(langid);
    parent_->set_restart_wizard(TRUE);
    parent_->PressButton(PSBTN_CANCEL);
  }

  return 0;
}

// Notifications

int SelectDevicePage::OnSetActive() {
  UpdateDialogControls();
  return 0;
}

// Private methods

void SelectDevicePage::InitLanguagesComboBox() {
  LANGID chinese_simplified = static_cast<LANGID>(
      ConvertDefaultLocale(MAKELANGID(LANG_CHINESE_SIMPLIFIED,
                                      SUBLANG_CHINESE_SIMPLIFIED)));
  LANGID chinese_traditional = static_cast<LANGID>(
      ConvertDefaultLocale(MAKELANGID(LANG_CHINESE_TRADITIONAL,
                                      SUBLANG_CHINESE_TRADITIONAL)));
  LANGID langids[] = {
      MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT),
      MAKELANGID(LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA),
      MAKELANGID(LANG_CATALAN, SUBLANG_CATALAN_CATALAN),
      MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT),
      MAKELANGID(LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC),
      MAKELANGID(LANG_DANISH, SUBLANG_DANISH_DENMARK),
      MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT),
      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_UK),
      MAKELANGID(LANG_ESTONIAN, SUBLANG_ESTONIAN_ESTONIA),
      MAKELANGID(LANG_FILIPINO, SUBLANG_FILIPINO_PHILIPPINES),
      MAKELANGID(LANG_FINNISH, SUBLANG_FINNISH_FINLAND),
      MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT),
      MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT),
      MAKELANGID(LANG_GREEK, SUBLANG_GREEK_GREECE),
      MAKELANGID(LANG_HEBREW, SUBLANG_HEBREW_ISRAEL),
      MAKELANGID(LANG_HINDI, SUBLANG_HINDI_INDIA),
      MAKELANGID(LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY),
      MAKELANGID(LANG_INDONESIAN, SUBLANG_INDONESIAN_INDONESIA),
      MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT),
      MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN),
      MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN),
      MAKELANGID(LANG_LATVIAN, SUBLANG_LATVIAN_LATVIA),
      MAKELANGID(LANG_LITHUANIAN, SUBLANG_LITHUANIAN),
      MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT),
      MAKELANGID(LANG_POLISH, SUBLANG_POLISH_POLAND),
      MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE),
      MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN),
      MAKELANGID(LANG_ROMANIAN, SUBLANG_ROMANIAN_ROMANIA),
      MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA),
      MAKELANGID(LANG_SERBIAN_NEUTRAL, SUBLANG_DEFAULT),
      MAKELANGID(LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA),
      MAKELANGID(LANG_SLOVENIAN, SUBLANG_SLOVENIAN_SLOVENIA),
      MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT),
      MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN),
      MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT),
      MAKELANGID(LANG_THAI, SUBLANG_THAI_THAILAND),
      MAKELANGID(LANG_TURKISH, SUBLANG_TURKISH_TURKEY),
      MAKELANGID(LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE),
      MAKELANGID(LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM),
      chinese_simplified,
      chinese_traditional};

  for (int index = 0; index < RTL_NUMBER_OF(langids); ++index) {
    if (!_Module.IsLanguageInstalled(langids[index]))
      continue;
    WCHAR language_name[MAX_PATH] = L"";
    VerLanguageName(langids[index],
                    language_name,
                    RTL_NUMBER_OF(language_name));
    int item = languages_combo_box_.InsertString(-1, language_name);
    languages_combo_box_.SetItemData(item, langids[index]);
    if (_Module.ui_langid() == langids[index]) {
      languages_combo_box_.SetCurSel(item);
    }
  }
}

void SelectDevicePage::InitDevicesListCtrl() {
  ListViewInsertColumn(&devices_list_ctrl_,
                       kDevicesListNameColumn,
                       IDS_DEVICES_LIST_COLUMN_NAME,
                       LVCFMT_LEFT,
                       200);

  ListViewInsertColumn(&devices_list_ctrl_,
                       kDevicesListDescColumn,
                       IDS_DEVICES_LIST_COLUMN_DESC,
                       LVCFMT_LEFT,
                       490);

  DWORD styles = LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP;
  devices_list_ctrl_.SetExtendedListViewStyle(styles, styles);

  CToolTipCtrl tooltip_ctrl = devices_list_ctrl_.GetToolTips();
  tooltip_ctrl.SetDelayTime(TTDT_AUTOPOP, 0x7FFF);
}

void SelectDevicePage::InsertDevicesIntoListCtrl() {
  int index = -1;
  ConfigData::DEVICES_LIST::iterator it;

  devices_list_ctrl_.DeleteAllItems();

  for (it = _Module.config()->devices()->begin();
       it != _Module.config()->devices()->end();
       ++it) {
    index = devices_list_ctrl_.AddItem(
                ++index,
                kDevicesListNameColumn,
                (*it).name());

    CString hwids;
    Device::HWID_LIST::iterator hwid_it;
    for (hwid_it = (*it).hwids()->begin();
         hwid_it != (*it).hwids()->end();
         ++hwid_it) {
      if (hwids.GetLength())
        hwids += L", ";
      hwids += *hwid_it;
    }

    // TODO(thieule): Display the HWIDs in a popup in the next revision.
    // For now, we'll just remove it fro the description field.
    CString description;
    description.Format(L"%s (%s, %s)",
                       (*it).desc(),
                       (*it).version(),
                       (*it).channel());
    devices_list_ctrl_.SetItemText(index,
                                    kDevicesListDescColumn,
                                    description);
    devices_list_ctrl_.SetItemData(index, reinterpret_cast<DWORD_PTR>(&(*it)));
  }

  devices_list_ctrl_.SetColumnWidth(kDevicesListNameColumn,
                                    LVSCW_AUTOSIZE_USEHEADER);
  devices_list_ctrl_.SetColumnWidth(kDevicesListDescColumn,
                                    LVSCW_AUTOSIZE_USEHEADER);

  devices_list_ctrl_.SelectItem(0);
}

void SelectDevicePage::UpdateDialogControls() {
  if (devices_list_ctrl_.GetSelectedCount() > 0) {
    parent_->SetWizardButtons(CROS_PSWIZB_LOCAL_IMAGE | PSWIZB_NEXT);
  } else {
    parent_->SetWizardButtons(CROS_PSWIZB_LOCAL_IMAGE);
  }
}
