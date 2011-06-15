// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPCONTROLLER_H_
#define APPCONTROLLER_H_
#pragma once

#import <AppKit/AppKit.h>
#include <sys/types.h>

@interface AppController : NSObject {
 @private
  IBOutlet NSWindow* window_;
  IBOutlet NSTabView* tabView_;

  // Tab: Select device model
  IBOutlet NSTextField* welcomeText_;
  IBOutlet NSTableView* imageTable_;

  // Tab: Select USB stick
  IBOutlet NSTextField* selectStickText_;
  IBOutlet NSTableView* stickTable_;

  // Tab: Work
  IBOutlet NSTextField* statusLine_;
  IBOutlet NSProgressIndicator* progressIndicator_;

  // Tab: Done
  IBOutlet NSTextField* congratsText_;

  // Other stuff
  NSURLConnection* configConnection_;
  NSMutableData* configData_;
  NSArray* images_;  // Array of dictionary of info
  BOOL isImageLocal_;
  NSDictionary* image_;  // valid if !isImageLocal; WEAK (member of images_)
  NSString* imagePath_;
  unsigned int urlIndex_;
  NSString* downloadPath_;
  off_t imageSize_;
  NSURLDownload* download_;
  DASessionRef arbitrationSession_;
  NSMutableArray* sticks_;  // Array of DADiskRefs
  struct Status {
    off_t progressBytes;
    const char* statusText;
    int attempt;
    BOOL progressIndeterminate;
  } status_;
  BOOL stopping_;

  // Helper function access
 @public
  NSMutableArray* claimedSticks_;  // Array of DADiskRefs
  DADiskRef selectedStick_;
  pthread_cond_t diskArbCondition_;
  pthread_mutex_t diskArbMutex_;
  enum {
    kDiskArbUnknown,
    kDiskArbFailed,
    kDiskArbSucceeded
  } diskArbSuccess_;
}

- (IBAction)nextTab:(id)sender;
- (IBAction)previousTab:(id)sender;
- (IBAction)done:(id)sender;
- (void)switchToTabAtIndex:(NSInteger)index;

- (IBAction)showHelp:(id)sender;

- (void)whineAtUser:(NSString*)errorName, ...;

// Tab: Select device model
@property (nonatomic, readonly) BOOL selectDeviceNextEnabled;

- (void)loadConfig;
- (void)parseConfig:(NSString*)configString;
- (NSDictionary*)parseStanza:(NSString*)stanza
               withArrayKeys:(NSSet*)arrayKeys;
- (NSComparisonResult)compareVersion:(NSString*)left
                           toVersion:(NSString*)right;
- (BOOL)isValidFilename:(NSString*)filename;
- (NSInteger)imageTableRowCount;
- (id)imageTableObjectValueForRow:(NSInteger)row;
- (NSString*)imageTableToolTipForRow:(NSInteger)row;
- (IBAction)selectLocalFile:(id)sender;
- (BOOL)panel:(id)sender shouldShowFilename:(NSString*)filename;
- (void)imageSelectionChanged;
- (IBAction)imageWasDoubleClicked:(id)sender;
- (BOOL)selectDeviceNext;

// Tab: Select USB stick
@property (nonatomic, readonly) BOOL selectUSBStickNextEnabled;
@property (nonatomic, readonly) BOOL insertUSBStickHidden;

- (void)diskAppeared:(DADiskRef)disk;
- (void)diskDisappeared:(DADiskRef)disk;
- (void)diskPeek:(DADiskRef)disk;
- (BOOL)isUSBStick:(DADiskRef)disk;
- (NSInteger)stickTableRowCount;
- (id)stickTableObjectValueForRow:(NSInteger)row;
- (void)stickSelectionChanged;
- (IBAction)stickWasDoubleClicked:(id)sender;
- (BOOL)selectUSBStickNext;
- (NSString*)descriptionForDisk:(DADiskRef)disk;
- (NSString*)sizeStringForValue:(double)size;

// Tab: Work
- (IBAction)knockItOff:(id)sender;
struct FailureInfo {
  const char* failureStep;
  enum {
    kNoErrorCodeAvailable,
    kErrnoError,
    kReturnValueError,
    kInternalError
  } errorDomain;
  int errorCode;
};
- (void)failWithInfo:(FailureInfo*)info;
// Clean up work cycle. If user stopped, info is nil; if error, info is NSData
// of struct FailureInfo.
- (void)cleanUp:(NSData*)info;
- (void)startWriteImage;
- (void)doDownload;
- (void)verify;
- (void)verifyComplete;
- (void)unpack;
- (void)unpackComplete;
- (void)writeImage;
- (void)writeImageComplete;
- (void)sendStatusUpdate;
- (void)updateStatus:(NSData*)value;  // NSData of struct Status
- (BOOL)workNext;

// Tab: Done

@end

#endif  // APPCONTROLLER_H_
