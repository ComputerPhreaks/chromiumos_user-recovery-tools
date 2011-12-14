// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_recovery.h"
#include "config_data.h"
#include "utils.h"

static const char kChannelKey[] = "channel";
static const char kDescKey[] = "desc";
static const char kFileKey[] = "file";
static const char kFileSizeKey[] = "filesize";
static const char kHwidKey[] = "hwid";
static const char kMd5Key[] = "md5";
static const char kNameKey[] = "name";
static const char kRecoveryToolUpdateKey[] = "recovery_tool_update";
static const char kRecoveryToolVersionKey[] = "recovery_tool_version";
static const char kSha1Key[] = "sha1";
static const char kUrlKey[] = "url";
static const char kVersionKey[] = "version";
static const char kZipFileSizeKey[] = "zipfilesize";

ConfigData::ConfigData() {
}

// Load and parse the specified configuration.
// Sample configuration file:
//
//   recovery_tool_version=0.9.2
//   recovery_tool_linux_version=0.9.2
//   recovery_tool_update=
//   
//   name=Chrome Notebook
//   version=0.10.156.50
//   desc=Recovery Installer for Chrome Notebook
//   channel=recovery
//   hwid=IEC MARIO FISH 2330
//   hwid=IEC MARIO PONY 6101
//   md5=8f1e5b3acc829fcc640a4f63ebb7e755
//   sha1=f9dc3e41eda4f50cdb6c304a95b3b840d59755f5
//   zipfilesize=335174186
//   file=chromeos_0.10.156.50_x86-mario_recovery_beta-channel_mp.bin
//   filesize=970999296
//   url=https://dl.google.com/.../chromeos_0.10.156.50...bin.zip
//
// See README_formats.txt for more information.
DWORD ConfigData::Load(__in const CString& config_file_name) {
  DWORD error;
  Device device;

  std::ifstream config_file;
  config_file.open(config_file_name, std::ifstream::in);
  if (!config_file) {
    error = ErrnoToWin32(errno);
    goto done;
  }

  DumpConfigFileToLog(config_file_name);

  while (!config_file.eof()) {
    std::string line;
    std::getline(config_file, line);

    std::vector<std::string> tokens = Split(line, "=");
    if (tokens.size() < 2) {
      if (!device.IsEmpty()) {
        devices_.push_back(device);
        device.Clear();
      }
      continue;
    }

    std::wstring wstr(tokens[1].length(), L'');
    copy(tokens[1].begin(), tokens[1].end(), wstr.begin());
    if (tokens[0] == kRecoveryToolVersionKey) {
      recovery_tool_version_ = wstr.c_str();
    } else if (tokens[0] == kRecoveryToolUpdateKey) {
      recovery_tool_update_ = wstr.c_str();
    } else if (tokens[0] == kNameKey) {
      device.set_name(wstr.c_str());
    } else if (tokens[0] == kDescKey) {
      device.set_desc(wstr.c_str());
    } else if (tokens[0] == kVersionKey) {
      device.set_version(wstr.c_str());
    } else if (tokens[0] == kHwidKey) {
      device.hwids()->push_back(wstr.c_str());
    } else if (tokens[0] == kChannelKey) {
      device.set_channel(wstr.c_str());
    } else if (tokens[0] == kZipFileSizeKey) {
      ULONGLONG size = _wtoi64(wstr.c_str());
      device.set_zipfile_size(max(size, 0));
    } else if (tokens[0] == kFileSizeKey) {
      ULONGLONG size = _wtoi64(wstr.c_str());
      device.set_file_size(max(size, 0));
    } else if (tokens[0] == kFileKey) {
      device.set_file(wstr.c_str());
    } else if (tokens[0] == kMd5Key) {
      device.set_md5(wstr.c_str());
    } else if (tokens[0] == kSha1Key) {
      device.set_sha1(wstr.c_str());
    } else if (tokens[0] == kUrlKey) {
      device.urls()->push_back(wstr.c_str());
    }
  }

  // Add any remaining config that was left out.
  if (!device.IsEmpty()) {
    devices_.push_back(device);
  }

  DumpDevices();

  error = ERROR_SUCCESS;

 done:
  return error;
}

// Private methods

std::vector<std::string> ConfigData::Split(__in const std::string& s,
                                            __in const std::string& delim) {
  std::vector<std::string> result;
  if (delim.empty() || s.empty() || s.size() <= delim.size()) {
    result.push_back(s);
    return result;
  }
  std::string::const_iterator substart = s.begin(), subend;
  subend = std::search(substart, s.end(), delim.begin(), delim.end());
  std::string temp(substart, subend);
  if (!temp.empty()) {
    result.push_back(temp);
    std::string temp_value(subend + delim.size(), s.end());
    result.push_back(temp_value);
  }
  return result;
}

void ConfigData::DumpConfigFileToLog(__in const CString& config_file_name) {
  std::ifstream config_file;
  config_file.open(config_file_name, std::ifstream::in);
  if (!config_file)
    return;

  _Module.Log(CROS_LOG_INFO, L"Config file content:");
  while (!config_file.eof()) {
    std::string line;
    std::getline(config_file, line);
    _Module.Log(CROS_LOG_INFO, L"     %S", line.c_str());
  }
}

void ConfigData::DumpDevices() {
  DEVICES_LIST::iterator it;
  for (it = devices_.begin(); it != devices_.end(); ++it)
    (*it).Dump();
}
