// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOSRECOVERY_CONFIGDATA_H_
#define CHROMEOSRECOVERY_CONFIGDATA_H_

// Recovery information of a single Chrome OS device.
class Device {
 public:
  typedef std::vector<CString> HWID_LIST;
  typedef std::vector<CString> URL_LIST;

  Device() {
    Clear();
  };

  // Resets the device information.
  void Clear() {
    hwids_.clear();
    urls_.clear();
  };

  // Returns TRUE if the device has not been initialized.
  BOOL IsEmpty() {
    return (name_.IsEmpty() || desc_.IsEmpty() || version_.IsEmpty() ||
            channel_.IsEmpty() || (hwids_.size() == 0) ||
            (urls_.size() == 0) || (file_size_ <= 0));
  };

  // Dumps the content of the object to the log file.
  void Dump() {
    _Module.Log(CROS_LOG_INFO, L"Device 0x%p dump:", this);
    _Module.Log(CROS_LOG_INFO, L"     Name:          %s", name_);
    _Module.Log(CROS_LOG_INFO, L"     Version:       %s", version_);
    _Module.Log(CROS_LOG_INFO, L"     Description:   %s", desc_);
    _Module.Log(CROS_LOG_INFO, L"     File size:     %I64u", file_size_);
    _Module.Log(CROS_LOG_INFO, L"     Zip file size: %I64u", zipfile_size_);
    _Module.Log(CROS_LOG_INFO, L"     File name:     %s", file_);
    _Module.Log(CROS_LOG_INFO, L"     MD5:           %s", md5_);
    _Module.Log(CROS_LOG_INFO, L"     SHA1:          %s", sha1_);
    _Module.Log(CROS_LOG_INFO, L"     Channel:       %s", channel_);

    HWID_LIST::iterator hwid_it;
    for (hwid_it = hwids_.begin(); hwid_it != hwids_.end(); ++hwid_it) {
      _Module.Log(CROS_LOG_INFO, L"     HWID:          %s", *hwid_it);
    }

    URL_LIST::iterator url_it;
    for (url_it = urls_.begin(); url_it != urls_.end(); ++url_it) {
      _Module.Log(CROS_LOG_INFO, L"     URL:           %s", *url_it);
    }
  }

  CString name() const { return name_; }
  void set_name(__in const CString& val) { name_ = val; }
  CString version() const { return version_; }
  void set_version(__in const CString& val) { version_ = val; }
  CString desc() const { return desc_; }
  void set_desc(__in const CString& val) { desc_ = val; }
  ULONGLONG file_size() const { return file_size_; }
  void set_file_size(__in ULONGLONG val) { file_size_ = val; }
  ULONGLONG zipfile_size() const { return zipfile_size_; }
  void set_zipfile_size(__in ULONGLONG val) { zipfile_size_ = val; }
  CString file() const { return file_; }
  void set_file(__in const CString& val) { file_ = val; }
  CString md5() const { return md5_; }
  void set_md5(__in const CString& val) { md5_ = val; }
  CString sha1() const { return sha1_; }
  void set_sha1(__in const CString& val) { sha1_ = val; }
  CString channel() const { return channel_; }
  void set_channel(__in const CString& val) { channel_ = val; }
  HWID_LIST* hwids() { return &hwids_; }
  URL_LIST* urls() { return &urls_; }

 private:
  // Device name.
  CString name_;

  // Recovery image version.
  CString version_;

  // Device description.
  CString desc_;

  // File size (uncompressed, in bytes).
  ULONGLONG file_size_;

  // Zip file size (in bytes).
  ULONGLONG zipfile_size_;

  // Image file name.
  CString file_;

  // MD5 hash of zip file.
  CString md5_;

  // SHA1 hash of zip file.
  CString sha1_;

  // Channel (dev, beta, etc).
  CString channel_;

  // List of HWIDs that applies to the recovery image.
  HWID_LIST hwids_;

  // List of URLs to download the recovery image from.
  URL_LIST urls_;
};

// Chrome OS devices recovery information.
class ConfigData {
 public:
  typedef std::list<Device> DEVICES_LIST;

  ConfigData();

  // Loads the recovery information from the specified file.
  DWORD Load(__in const CString& config_file_name);

  DEVICES_LIST* devices() { return &devices_; }
  CString recovery_tool_version() { return recovery_tool_version_; }
  CString recovery_tool_update() { return recovery_tool_update_; }

 private:
  // Splits |s| into strings based on the specified delimiter.
  // The substrings are returned.
  std::vector<std::string> Split(__in const std::string& s,
                                 __in const std::string& delim);

  // Dumps the content of the config file to the log file.
  void DumpConfigFileToLog(__in const CString& config_file_name);

  // Dumps all device configurations to the log file.
  void DumpDevices();

  // List of device configurations.
  DEVICES_LIST devices_;
  
  // Recovery tool version.
  CString recovery_tool_version_;

  // Message to display if the recovery tool version doesn't match.
  CString recovery_tool_update_;

  DISALLOW_COPY_AND_ASSIGN(ConfigData);
};

#endif  // CHROMEOSRECOVERY_CONFIGDATA_H_
