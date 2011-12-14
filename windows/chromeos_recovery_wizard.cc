// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "credits_dlg.h"

const int ChromeOSRecoveryWizard::kSelectDevicePageIndex = 0;
const int ChromeOSRecoveryWizard::kSelectUSBPageIndex = 1;
const int ChromeOSRecoveryWizard::kDownloadPageIndex = 2;
const int ChromeOSRecoveryWizard::kWriteImagePageIndex = 3;
const int ChromeOSRecoveryWizard::kFinishPageIndex = 4;

ChromeOSRecoveryWizard::ChromeOSRecoveryWizard(__in HWND parent)
    : CPropertySheetImpl<ChromeOSRecoveryWizard>(0U, 0, parent),
      select_device_page_(this),
      select_usb_page_(this),
      download_page_(this),
      write_image_page_(this),
      finish_page_(this),
      restart_wizard_(FALSE) {
  m_psh.dwFlags |= PSH_USEHICON;
  if (!_Module.is_win_xp_or_older())
    m_psh.dwFlags |= PSH_USEPAGELANG;
  m_psh.hIcon = LoadIcon(_Module.m_hInstResource,
                         MAKEINTRESOURCE(IDR_MAINFRAME));

  SetWizardMode();

  AddPage(select_device_page_);
  AddPage(select_usb_page_);
  AddPage(download_page_);
  AddPage(write_image_page_);
  AddPage(finish_page_);
}

// Overrides

void ChromeOSRecoveryWizard::SetWizardButtons(DWORD flags) {
  if (flags & CROS_PSWIZB_LOCAL_IMAGE)
    local_image_button_.ShowWindow(SW_SHOW);
  else
    local_image_button_.ShowWindow(SW_HIDE);

  flags &= ~CROS_PSWIZB_LOCAL_IMAGE;
  CPropertySheetImpl<ChromeOSRecoveryWizard>::SetWizardButtons(flags);
}

void ChromeOSRecoveryWizard::OnSheetInitialized() {
  CMenu sub_menu;
  sub_menu.CreateMenu();
  CString menu_text(reinterpret_cast<LPCWSTR>(IDS_MENU_CREDITS));
  sub_menu.AppendMenu(MF_STRING, IDC_CREDITS, menu_text);
  CMenu system_menu = GetSystemMenu(FALSE);
  menu_text.LoadString(IDS_MENU_ABOUT);
  system_menu.AppendMenu(MF_POPUP, sub_menu, menu_text);

  // We'll set the final position and size of the Local Image button
  // in OnWindowPosChanged().
  CRect rect;
  CString local_image_text;
  local_image_text.LoadString(IDS_LOCAL_IMAGE);
  local_image_button_.Create(m_hWnd,
                             rect,
                             local_image_text,
                             BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP,
                             0,
                             IDC_LOCAL_IMAGE);
  local_image_button_.SetFont(GetFont());
}

LRESULT ChromeOSRecoveryWizard::OnWindowPosChanged(UINT /*msg*/,
                                                   WPARAM /*wparam*/,
                                                   LPARAM /*lparam*/,
                                                   BOOL& /*handled*/) {
  CRect active_page_rect;
  CWindow active_page = GetActivePage();
  if (!active_page.IsWindow())
    return 0;
  active_page.GetWindowRect(active_page_rect);

  CRect rect;
  CWindow next_button = GetDlgItem(ID_WIZNEXT);
  next_button.GetWindowRect(rect);

  CWindow desktop = GetDesktopWindow();
  desktop.MapWindowPoints(m_hWnd, rect);
  desktop.MapWindowPoints(m_hWnd, active_page_rect);

  int width = rect.Width();
  rect.left = active_page_rect.left;
  rect.right = active_page_rect.left + width;
  local_image_button_.MoveWindow(rect);

  return 0;
}

void ChromeOSRecoveryWizard::OnSysCommand(__in UINT id, __in CPoint /*point*/) {
  if (id == IDC_CREDITS) {
    CreditsDlg credits_dlg;
    credits_dlg.DoModal();
  } else {
    SetMsgHandled(FALSE);
  }
}

LRESULT ChromeOSRecoveryWizard::OnLocalImage(__in WORD code,
                                             __in WORD id,
                                             __in HWND ctrl,
                                             __inout BOOL& handled) {
  NMHDR nmhdr = {m_hWnd, 0, CROS_PSN_LOCAL_IMAGE};
  ::SendMessage(GetActivePage(),
                WM_NOTIFY,
                0,
                reinterpret_cast<LPARAM>(&nmhdr));
  return 0;
}

// Accessors and mutators

SelectDevicePage* ChromeOSRecoveryWizard::select_device_page() {
  return &select_device_page_;
}

SelectUSBPage* ChromeOSRecoveryWizard::select_usb_page() {
  return &select_usb_page_;
}

DownloadPage* ChromeOSRecoveryWizard::download_page() {
  return &download_page_;
}

WriteImagePage* ChromeOSRecoveryWizard::write_image_page() {
  return &write_image_page_;
}

FinishPage* ChromeOSRecoveryWizard::finish_page() {
  return &finish_page_;
}

BOOL ChromeOSRecoveryWizard::restart_wizard() {
  return restart_wizard_;
}

void ChromeOSRecoveryWizard::set_restart_wizard(BOOL restart) {
  restart_wizard_ = restart;
}
