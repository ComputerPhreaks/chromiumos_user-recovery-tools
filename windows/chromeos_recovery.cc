// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include "chromeos_recovery.h"
#include "chromeos_recovery_wizard.h"
#include "config_data.h"
#include "download_config_dlg.h"
#include "resource.h"
#include "utils.h"

ChromeOSRecoveryWizardApp _Module;

ChromeOSRecoveryWizardApp::ChromeOSRecoveryWizardApp() : config_(NULL) {
  ui_langid_ = GetUserDefaultUILanguage();
  recovery_conf_url_ =
      L"https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
  is_win_xp_or_older_ = TRUE;
}

ChromeOSRecoveryWizardApp::~ChromeOSRecoveryWizardApp() {
  if (!log_file_name_.IsEmpty()) {
    log_file_.Close();
    ::DeleteFile(log_file_name_);
  }
  
  if (config_)
    delete config_;
}

HRESULT ChromeOSRecoveryWizardApp::Init(__in ATL::_ATL_OBJMAP_ENTRY* obj_map,
                                        __in HINSTANCE instance,
                                        __in const GUID* lib_id) {
  HRESULT hr = CAppModule::Init(obj_map, instance, lib_id);
  if (FAILED(hr))
    goto done;

  log_file_name_ = GetTempFileName();
  hr = log_file_.Create(log_file_name_,
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL);
  if (FAILED(hr))
    goto done;

  int num_args = 0;
  WCHAR** argv = CommandLineToArgvW(GetCommandLine(), &num_args);
  if (!argv) {
    hr = HRESULT_FROM_WIN32(GetLastError());
    goto done;
  }
  for (int index = 0; index < num_args; ++index) {
    CString arg = argv[index];
    if (!arg.CompareNoCase(L"/u") && index + 1 < num_args) {
      recovery_conf_url_ = argv[index + 1];
    }
  }
  LocalFree(argv);

  BOOL success = EnumUILanguages(EnumUILanguagesCallback,
                                 0,
                                 reinterpret_cast<LONG_PTR>(this));
  if (!success) {
    hr = HRESULT_FROM_WIN32(GetLastError());
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to enumerate UI languages (%d)",
                hr);
    goto done;
  }

  DWORD error = DownloadConfig();
  hr = HRESULT_FROM_WIN32(error);

  OSVERSIONINFOEX version;
  DWORDLONG condition_mask = 0;
  ZeroMemory(&version, sizeof(version));
  version.dwOSVersionInfoSize = sizeof(version);
  version.dwMajorVersion = 6;
  VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, VER_LESS);
  VER_SET_CONDITION(condition_mask, VER_MINORVERSION, VER_LESS);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMAJOR, VER_LESS);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMINOR, VER_LESS);
  is_win_xp_or_older_ = VerifyVersionInfo(&version,
                                          VER_MAJORVERSION | VER_MINORVERSION |
                                          VER_SERVICEPACKMAJOR |
                                          VER_SERVICEPACKMINOR,
                                          condition_mask);

 done:
  return hr;
}

BOOL ChromeOSRecoveryWizardApp::IsLanguageInstalled(__in LANGID langid) {
    return (installed_ui_languages_.find(langid) !=
            installed_ui_languages_.end());
}

void ChromeOSRecoveryWizardApp::Log(__in int level,
                                    __in const WCHAR* format,
                                    ...) {
  CString message;
  va_list args;
  va_start(args, format);
  message.FormatV(format, args);
  va_end(args);

  CString level_str;
  switch (level) {
    case CROS_LOG_INFO:   level_str = L"INFO";  break;
    case CROS_LOG_WARN:   level_str = L"WARN";  break;
    case CROS_LOG_ERROR:  level_str = L"ERROR"; break;
  }

  SYSTEMTIME time;
  GetLocalTime(&time);
  WCHAR time_str[64] = L"";
  GetTimeFormat(LOCALE_USER_DEFAULT,
                0,
                &time,
                NULL,
                time_str,
                RTL_NUMBER_OF(time_str));

  CString log_text;
  log_text.Format(L"%s\t%s: %s\r\n", time_str, level_str, message);

  log_file_.Write(log_text, log_text.GetLength() * sizeof(WCHAR));
}

LANGID ChromeOSRecoveryWizardApp::ui_langid() {
  return ui_langid_;
}

ConfigData* ChromeOSRecoveryWizardApp::config() {
  return config_;
}

void ChromeOSRecoveryWizardApp::set_ui_langid(__in LANGID langid) {
  SetThreadUILanguage(langid);
  ui_langid_ = langid;
}

CString ChromeOSRecoveryWizardApp::log_file_name() {
  return log_file_name_;
}

CString ChromeOSRecoveryWizardApp::recovery_conf_url() {
  return recovery_conf_url_;
}

BOOL ChromeOSRecoveryWizardApp::is_win_xp_or_older() {
  return is_win_xp_or_older_;
}

// Private methods

BOOL CALLBACK ChromeOSRecoveryWizardApp::EnumUILanguagesCallback(
    __in LPTSTR language_string,
    __in LONG_PTR param) {
  ChromeOSRecoveryWizardApp* t =
      reinterpret_cast<ChromeOSRecoveryWizardApp*>(param);
  errno = 0;
  LANGID langid = static_cast<LANGID>(wcstol(language_string, NULL, 16));
  if (errno == 0)
    t->installed_ui_languages_[langid] = TRUE;
  return TRUE;
}

DWORD ChromeOSRecoveryWizardApp::DownloadConfig() {
  DWORD error;
  DownloadConfigDlg download_config_dlg(_Module.recovery_conf_url());
  CString config_file;

  config_ = new(std::nothrow) ConfigData;
  if (!config_) {
    error = ERROR_OUTOFMEMORY;
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to create config data object (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

  int ret = download_config_dlg.DoModal(NULL, IDS_DOWNLOADING_CONFIG);
  if (ret != IDOK) {
    error = ERROR_CANCELLED;
    goto done;
  }
  config_file = download_config_dlg.downloaded_file();

  error = config_->Load(config_file);
  if (error != ERROR_SUCCESS) {
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to load configuration from file %s",
                config_file);
    DisplayInternalErrorMessage();
    goto done;
  }

  if (config_->recovery_tool_version() != CROS_RECOVERY_TOOL_VERSION) {
    error = ERROR_REVISION_MISMATCH;
    _Module.Log(CROS_LOG_ERROR,
                L"Recovery tool version mismatch (version=%s, expected=%s)",
                config_->recovery_tool_version(),
                CROS_RECOVERY_TOOL_VERSION);
    FmtMessageBox(NULL,
                  MB_OK | MB_ICONERROR | CROS_MB_INCLUDE_LOG_INFO,
                  IDS_RECOVERY_TOOL_UPDATE_NEEDED,
                  config_->recovery_tool_update());
    goto done;
  }

 done:
  if (!config_file.IsEmpty())
    ::DeleteFile(config_file);

  return error;
}

static DWORD EnablePrivilege(__in const WCHAR* privilege_name) {
  DWORD error = ERROR_SUCCESS;
  HANDLE token = NULL;

  TOKEN_PRIVILEGES token_priv;
  ZeroMemory(&token_priv, sizeof(token_priv));
  token_priv.PrivilegeCount = 1;

  BOOL success = OpenProcessToken(GetCurrentProcess(),
                                  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                  &token);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR, L"Failed to open proces token (%d)", error);
    DisplayInternalErrorMessage();
    goto done;
  }

  success = LookupPrivilegeValue(NULL,
                                 privilege_name,
                                 &token_priv.Privileges[0].Luid);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to lookup privilege value (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }
  token_priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  success = AdjustTokenPrivileges(token,
                                  FALSE,
                                  &token_priv,
                                  sizeof(token_priv),
                                  NULL,
                                  NULL);
  if (!success) {
    error = GetLastError();
    _Module.Log(CROS_LOG_ERROR,
                L"Failed to adjust token privileges (%d)",
                error);
    DisplayInternalErrorMessage();
    goto done;
  }

 done:
  if (token)
    CloseHandle(token);

  return error;
}

int WINAPI _tWinMain(HINSTANCE instance,
                     HINSTANCE /*prev_instance*/,
                     LPTSTR /*cmd_line*/,
                     int /*show*/) {
  DWORD error;
  BOOL module_initialized = FALSE;

  HRESULT hr = ::CoInitialize(NULL);
  ATLASSERT(SUCCEEDED(hr));

  hr = CoInitializeSecurity(NULL,
                            -1,
                            NULL,
                            NULL,
                            RPC_C_AUTHN_LEVEL_DEFAULT,
                            RPC_C_IMP_LEVEL_IMPERSONATE,
                            NULL,
                            EOAC_NONE,
                            NULL);
  ATLASSERT(SUCCEEDED(hr));

  // this resolves ATL window thunking problem when
  // Microsoft Layer for Unicode (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS);

  hr = _Module.Init(NULL, instance);
  if (FAILED(hr)) {
    FmtMessageBox(NULL,
                  MB_OK | MB_ICONERROR,
                  IDS_ERROR_INTERNAL);
    goto done;
  }

  module_initialized = TRUE;
  _Module.Log(CROS_LOG_INFO, L"Starting");

  error = EnablePrivilege(SE_MANAGE_VOLUME_NAME);
  if (error != ERROR_SUCCESS)
    goto done;

  DWORD selected_usb_disk_index = -1;
  while (TRUE) {
    ChromeOSRecoveryWizard recovery_wizard;
    int ret = recovery_wizard.DoModal();
    if (recovery_wizard.restart_wizard())
      continue;
    error = (ret == IDOK) ? ERROR_SUCCESS : ERROR_CANCELLED;
    selected_usb_disk_index =
      recovery_wizard.select_usb_page()->selected_usb_disk_index();
    break;
  }

#ifndef _DEBUG
  if (selected_usb_disk_index != -1) {
    EjectUsbDisk(selected_usb_disk_index);
  }
#endif

 done:
  if (module_initialized)
    _Module.Term();
  ::CoUninitialize();

  _Module.Log(CROS_LOG_INFO, L"Stopping");
  return error;
}
