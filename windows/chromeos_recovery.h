// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_CHROMEOS_RECOVERY_H_
#define CHROMEOSRECOVERY_CHROMEOS_RECOVERY_H_

#define WM_CROS_BASE                (WM_USER + 1793)
#define WM_CROS_DOWNLOAD_DONE       (WM_CROS_BASE + 1)
#define WM_CROS_VERIFY_DONE         (WM_CROS_BASE + 2)
#define WM_CROS_UNZIP_DONE          (WM_CROS_BASE + 3)
#define WM_CROS_WRITE_IMAGE_DONE    (WM_CROS_BASE + 4)

#define CROS_LOG_INFO               1
#define CROS_LOG_WARN               2
#define CROS_LOG_ERROR              3

#define CROS_RECOVERY_TOOL_VERSION  L"0.9.2"

class ConfigData;

// Main WTL application module.
class ChromeOSRecoveryWizardApp : public CAppModule {
 public:
  ChromeOSRecoveryWizardApp();
  virtual ~ChromeOSRecoveryWizardApp();

  // Initializes the application.
  HRESULT Init(__in ATL::_ATL_OBJMAP_ENTRY* obj_map,
               __in HINSTANCE instance,
               __in const GUID* lib_id = NULL);

  // Writes a message to the log file.
  void Log(__in int level, __in const WCHAR* format, ...);

  // Checks to see if the specified language is installed.
  BOOL IsLanguageInstalled(__in LANGID langid);

  LANGID ui_langid();
  ConfigData* config();
  void set_ui_langid(__in LANGID langid);
  CString log_file_name();
  CString recovery_conf_url();
  BOOL is_win_xp_or_older();

 private:
  // Callback used to enumerate UI languages.
  static BOOL CALLBACK EnumUILanguagesCallback(__in LPTSTR language_string,
                                               __in LONG_PTR param);

  // Downloads the Chrome OS recovery config information.
  DWORD DownloadConfig();

  // Current UI language.
  LANGID ui_langid_;

  // List of installed UI languages.
  typedef std::map<LANGID,BOOL> UI_LANGUAGES;
  UI_LANGUAGES installed_ui_languages_;

  // Chrome OS recovery configuration information.
  ConfigData* config_;

  // Full path to the log file.
  CString log_file_name_;

  // File object used to access the log file.
  CAtlFile log_file_;

  // URL that points to where we can get the Chrome OS recovery information.
  CString recovery_conf_url_;

  // Flag indicating whether we're on XP.
  BOOL is_win_xp_or_older_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSRecoveryWizardApp);
};

extern ChromeOSRecoveryWizardApp _Module;

#endif  // CHROMEOSRECOVERY_CHROMEOS_RECOVERY_H_
