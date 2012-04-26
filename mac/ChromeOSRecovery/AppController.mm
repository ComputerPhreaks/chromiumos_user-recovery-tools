// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppController.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <objc/objc-runtime.h>
#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#include <util.h>

#include <vector>

#include "DockTile.h"
#include "eintr_wrapper.h"
#include "scoped_ptr.h"

namespace {

NSString* const kConfigurationURLKey = @"ChromeOSRecoveryConfigurationURL";
NSString* const kConfigurationURLString =
    @"https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
// TODO: Soon this value will be in the config file. When that is the case, use
// that value with this as a backup.
NSString* const kRecoveryToolURLString =
    @"http://www.google.com/chromeos/recovery";
NSString* const kHelpURLString =
    @"http://www.google.com/chromeos/recovery";

NSString* const kConfigFileVersion = @"recovery_tool_mac_version";
NSString* const kConfigUpgradeMessage = @"recovery_tool_update";
NSString* const kConfigChannel = @"channel";
NSString* const kConfigName = @"name";
NSString* const kConfigFile = @"file";
NSString* const kConfigHWID = @"hwid";
NSString* const kConfigSHA1 = @"sha1";
NSString* const kConfigVersion = @"version";
NSString* const kConfigDesc = @"desc";
NSString* const kConfigZipSize = @"zipfilesize";
NSString* const kConfigImageSize = @"filesize";
NSString* const kConfigURL = @"url";

enum {
  kInternalErrorUnexpectedEOF = 1
};

const NSTimeInterval kNSTaskPollingInterval = 0.2;

enum {
  kSelectDeviceTab = 0,
  kSelectUSBStickTab,
  kWorkTab,
  kDoneTab
};

void DiskAppeared(DADiskRef disk, void* context) {
  AppController* appController = (AppController*)context;

  [appController diskAppeared:disk];
}

void DiskDisappeared(DADiskRef disk, void* context) {
  AppController* appController = (AppController*)context;

  [appController diskDisappeared:disk];
}

void DiskPeek(DADiskRef disk, void* context) {
  AppController* appController = (AppController*)context;

  [appController diskPeek:disk];
}

void DiskUnmounted(DADiskRef disk, DADissenterRef dissenter, void* context) {
  AppController* appController = (AppController*)context;

  pthread_mutex_lock(&appController->diskArbMutex_);
  appController->diskArbSuccess_ =
      dissenter ? kDiskArbFailed : kDiskArbSucceeded;
  pthread_mutex_unlock(&appController->diskArbMutex_);
  pthread_cond_signal(&appController->diskArbCondition_);
}

void DiskEjected(DADiskRef disk, DADissenterRef dissenter, void* context) {
  AppController* appController = (AppController*)context;

  pthread_mutex_lock(&appController->diskArbMutex_);
  appController->diskArbSuccess_ =
      dissenter ? kDiskArbFailed : kDiskArbSucceeded;
  pthread_mutex_unlock(&appController->diskArbMutex_);
  pthread_cond_signal(&appController->diskArbCondition_);
}

void DiskClaimed(DADiskRef disk, DADissenterRef dissenter, void* context) {
  AppController* appController = (AppController*)context;

  pthread_mutex_lock(&appController->diskArbMutex_);
  appController->diskArbSuccess_ =
      dissenter ? kDiskArbFailed : kDiskArbSucceeded;
  pthread_mutex_unlock(&appController->diskArbMutex_);
  pthread_cond_signal(&appController->diskArbCondition_);
}

DADissenterRef DiskClaimRevoked(DADiskRef disk, void* context) {
  CFStringRef reason =
      CFSTR("Hi. Sorry to bother you, but I'm busy overwriting the entire disk "
            "here. There's nothing to claim but the smoldering ruins of bytes "
            "that were in flash memory. Trust me, it's nothing that you want. "
            "All the best. Toodles!");
  return DADissenterCreate(kCFAllocatorDefault, kDAReturnBusy, reason);
}

void ProtectiveDiskClaimed(DADiskRef disk, DADissenterRef dissenter,
                           void* context) {
  AppController* appController = (AppController*)context;

  if (!dissenter)
    [appController->claimedSticks_ addObject:(id)disk];
}

DADissenterRef ProtectiveDiskClaimRevoked(DADiskRef disk, void* context) {
  AppController* appController = (AppController*)context;
  if (disk == appController->selectedStick_)
    return DiskClaimRevoked(disk, context);

  [appController->claimedSticks_ removeObject:(id)disk];
  return NULL;
}

id DictLookup(CFDictionaryRef dict, CFStringRef key) {
  return (id)[(NSDictionary*)dict objectForKey:(NSString*)key];
}

struct NSAutoreleasePoolDestroy {
  void operator()(NSAutoreleasePool* pool) const {
    [pool drain];
  }
};
typedef scoped_ptr_malloc<NSAutoreleasePool, NSAutoreleasePoolDestroy>
    ScopedNSAutoreleasePool;

struct PThreadMutexUnlock {
  void operator()(pthread_mutex_t* mutex) const {
    if (mutex)
      pthread_mutex_unlock(mutex);
  }
};
typedef scoped_ptr_malloc<pthread_mutex_t, PThreadMutexUnlock>
    ScopedPThreadMutexLock;

struct PThreadMutexDestroy {
  void operator()(pthread_mutex_t* mutex) const {
    if (mutex)
      pthread_mutex_destroy(mutex);
  }
};
typedef scoped_ptr_malloc<pthread_mutex_t, PThreadMutexDestroy>
    ScopedPThreadMutexOwner;

struct PThreadConditionDestroy {
  void operator()(pthread_cond_t* cond) const {
    if (cond)
      pthread_cond_destroy(cond);
  }
};
typedef scoped_ptr_malloc<pthread_cond_t, PThreadConditionDestroy>
    ScopedPThreadConditionOwner;

struct DADiskUnclaimDoer {
  void operator()(DADiskRef disk) const {
    if (disk)
      DADiskUnclaim(disk);
  }
};
typedef scoped_ptr_malloc<__DADisk, DADiskUnclaimDoer>
    ScopedDADiskClaim;

NSString* CommonPrefixOfStringArray(NSArray* array) {
  NSUInteger items = [array count];
  if (!items)
    return @"";

  NSString* firstString = [array objectAtIndex:0];
  NSString* prefixSoFar = @"";
  for (NSUInteger index = 1; index <= [firstString length]; ++index) {
    NSString* possiblePrefix = [firstString substringToIndex:index];
    for (NSUInteger item = 1; item < items; ++item) {
      if (![[array objectAtIndex:item] hasPrefix:possiblePrefix]) {
        return prefixSoFar;
      }
    }
    prefixSoFar = possiblePrefix;
  }
  return prefixSoFar;
}

NSString* SizeStringForValue(double size) {
  NSArray* sizes = [NSArray arrayWithObjects:
                    NSLocalizedString(@"Size Bytes", nil),
                    NSLocalizedString(@"Size Kilobytes", nil),
                    NSLocalizedString(@"Size Megabytes", nil),
                    NSLocalizedString(@"Size Gigabytes", nil),
                    NSLocalizedString(@"Size Terabytes", nil),
                    NSLocalizedString(@"Size Petabytes", nil),
                    nil];

  unsigned int dimension = 0;
  const int kKilo = 1000;  // we're doing New Apple Style sizes
  while (size > kKilo && dimension < [sizes count] - 1) {
    size /= kKilo;
    dimension++;
  }

  return [NSString stringWithFormat:@"%.2f%@",
          size, [sizes objectAtIndex:dimension]];
}

}  // namespace

@implementation AppController

- (void)awakeFromNib {
  [stickTable_ setDoubleAction:@selector(stickWasDoubleClicked:)];
  [stickTable_ setTarget:self];

  sticks_ = [[NSMutableArray alloc] init];
  claimedSticks_ = [[NSMutableArray alloc] init];

  arbitrationSession_ = DASessionCreate(kCFAllocatorDefault);
  if (!arbitrationSession_) {
    // DASessionCreate is not documented to fail, but if it does we can't run.
    [self whineAtUser:@"NoDiskArb"];
    [NSApp terminate:self];
    return;
  }

  DARegisterDiskAppearedCallback(arbitrationSession_,
                                 kDADiskDescriptionMatchMediaWhole,
                                 DiskAppeared,
                                 self);

  DARegisterDiskDisappearedCallback(arbitrationSession_,
                                    kDADiskDescriptionMatchMediaWhole,
                                    DiskDisappeared,
                                    self);

  DARegisterDiskPeekCallback(arbitrationSession_,
                             NULL,
                             0,
                             DiskPeek,
                             self);

  DASessionScheduleWithRunLoop(arbitrationSession_,
                               CFRunLoopGetMain(),
                               kCFRunLoopDefaultMode);

  // Cheesy RTL hackery to make things look decent; real tools would be nice.
  if ([NSLocalizedString(@"UI Is RTL", nil) isEqualToString:@"YES"]) {
    // Flip text fields if RTL.
    [welcomeText_ setAlignment:NSRightTextAlignment];
    [selectStickText_ setAlignment:NSRightTextAlignment];
    [statusLine_ setAlignment:NSRightTextAlignment];
    [congratsText_ setAlignment:NSRightTextAlignment];

    imageComboBox_ = imageComboBoxRTL_;
    [imageComboBoxLTR_ removeFromSuperview];
  } else {
    imageComboBox_ = imageComboBoxLTR_;
    [imageComboBoxRTL_ removeFromSuperview];
  }


  [window_ center];
  // The order in which objects are woken from the nib is undefined; if the
  // window is shown now it may not yet be localized. Wait.
  [window_ performSelector:@selector(makeKeyAndOrderFront:)
                withObject:self
                afterDelay:0];

  [self loadConfig];
}

- (void)dealloc {
  if (selectedStick_)
    CFRelease(selectedStick_);
  [sticks_ release];
  for (id disk in claimedSticks_)
    DADiskUnclaim((DADiskRef)disk);
  [claimedSticks_ release];
  if (arbitrationSession_) {
    DASessionUnscheduleFromRunLoop(arbitrationSession_,
                                   CFRunLoopGetMain(),
                                   kCFRunLoopDefaultMode);
    CFRelease(arbitrationSession_);
  }
  [download_ release];
  [downloadPath_ release];
  [imagePath_ release];
  [images_ release];
  [configData_ release];
  [configConnection_ release];

  [super dealloc];
}

- (BOOL)isRTL {
  return [NSLocalizedString(@"UI Is RTL", nil) isEqualToString:@"YES"];
}

- (IBAction)nextTab:(id)sender {
  NSTabViewItem* currentTabView = [tabView_ selectedTabViewItem];
  NSInteger currentTab = [tabView_ indexOfTabViewItem:currentTabView];

  // By default, clicking "Next" on a tab will take you to the next. A tab may
  // customize this by implementing a method -[tabname]Next returning a BOOL
  // that specifies if the next tab should be moved to (YES means proceed).
  NSString* nextSelectorString =
      [NSString stringWithFormat:@"%@Next", [currentTabView identifier]];
  SEL nextSelector = NSSelectorFromString(nextSelectorString);

  BOOL shouldAdvance = YES;
  if ([self respondsToSelector:nextSelector]) {
    // See http://www.red-sweater.com/blog/320/abusing-objective-c-with-class
    // which refers to
    // http://www.cocoabuilder.com/archive/cocoa/156384-objc-msgsend-problems-on-x86.html#156604
    typedef BOOL (*ImplReturningBOOL)(id, SEL);
    ImplReturningBOOL sender = (ImplReturningBOOL)objc_msgSend;
    shouldAdvance = sender(self, nextSelector);
  }

  if (shouldAdvance)
    [self switchToTabAtIndex:currentTab + 1];
}

- (IBAction)previousTab:(id)sender {
  NSInteger currentTab =
      [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]];

  [self switchToTabAtIndex:currentTab - 1];
}

- (IBAction)done:(id)sender {
  [NSApp terminate:sender];
}

- (void)switchToTabAtIndex:(NSInteger)index {
  // In the nib, each NSTabViewItem has a view with a single NSView child of the
  // desired size of the window. Switch while resizing the window.
  NSView* newTabViewItemView = [[tabView_ tabViewItemAtIndex:index] view];
  assert([[newTabViewItemView subviews] count] == 1);
  NSView* newContainerView = [[newTabViewItemView subviews] objectAtIndex:0];
  NSView* oldTabViewItemView = [[tabView_ selectedTabViewItem] view];
  assert([[oldTabViewItemView subviews] count] == 1);
  NSView* oldContainerView = [[oldTabViewItemView subviews] objectAtIndex:0];

  // Get a global delta.
  NSRect oldRect = [oldContainerView convertRect:[oldContainerView bounds]
                                          toView:nil];
  NSRect newRect = [newContainerView convertRect:[newContainerView bounds]
                                          toView:nil];
  CGFloat delta = NSHeight(newRect) - NSHeight(oldRect);
  NSRect windowRect = [window_ frame];
  windowRect.origin.y -= delta;
  windowRect.size.height += delta;

  // Animate.
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:0.2];  // add a bit of zip
  [[window_ animator] setFrame:windowRect display:YES];
  [[tabView_ animator] selectTabViewItemAtIndex:index];
  [NSAnimationContext endGrouping];
}

- (IBAction)showHelp:(id)sender {
  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:kHelpURLString]];
}

- (void)whineAtUser:(NSString*)errorName, ... {
  NSString* message =
      NSLocalizedString([errorName stringByAppendingString:@" Message"], nil);
  NSString* informative =
      NSLocalizedString([errorName stringByAppendingString:@" Informative"],
                        nil);

  const unichar ch = 0x2029;  // U+2029 (PARAGRAPH SEPARATOR)
  NSString* delim = [NSString stringWithCharacters:&ch length:1];
  NSString* both = [NSString stringWithFormat:@"%@%@%@",
                                              message, delim, informative];

  va_list args;
  va_start(args, errorName);
  both = [[[NSString alloc] initWithFormat:both
                                 arguments:args] autorelease];
  va_end(args);

  NSArray* bothArray = [both componentsSeparatedByString:delim];
  message = [bothArray objectAtIndex:0];
  informative = [bothArray objectAtIndex:1];

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:message];
  [alert setInformativeText:informative];

  [alert addButtonWithTitle:
      NSLocalizedString([errorName stringByAppendingString:@" OK"], nil)];
  [alert runModal];
  return;
}

#pragma mark NSTableViewDelegate

- (BOOL)tableView:(NSTableView*)tableView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
              row:(NSInteger)rowIndex {
  return NO;
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  [self stickSelectionChanged];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  return [self stickTableRowCount];
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
            row:(NSInteger)row {
  return [self stickTableObjectValueForRow:row];
}

#pragma mark NSComboBoxDelegate

- (void)comboBoxWillDismiss:(NSNotification*)notification {
  imageComboBoxPopped_ = NO;
}

- (void)comboBoxWillPopUp:(NSNotification*)notification {
  imageComboBoxPopped_ = YES;
}

- (void)controlTextDidChange:(NSNotification*)notification {
  [self imageComboTextChanged];
}

- (NSArray*)control:(NSControl*)control
           textView:(NSTextView*)textView
        completions:(NSArray*)words
forPartialWordRange:(NSRange)charRange
indexOfSelectedItem:(NSInteger*)index {
  return [NSArray array];  // no weird completions, please
}

#pragma mark Select Device

@synthesize loadingConfigFinished = loadingConfigFinished_;

- (void)loadConfig {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* configString =
      [bundle objectForInfoDictionaryKey:kConfigurationURLKey];
  if (!configString)
    configString = kConfigurationURLString;

  configData_ = [[NSMutableData alloc] init];
  NSURL* configURL = [NSURL URLWithString:configString];
  NSURLRequest* request = [NSURLRequest requestWithURL:configURL];
  configConnection_ = [[NSURLConnection alloc] initWithRequest:request
                                                      delegate:self
                                              startImmediately:YES];
  if (!configConnection_) {
    NSString* errorString =
        NSLocalizedString(@"CantGetConfig Error NoConnection", nil);
    [self whineAtUser:@"CantGetConfig", errorString];
    return;
  }
}

- (void)connection:(NSURLConnection*)connection
  didFailWithError:(NSError*)error {
  NSLog(@"Config file download failed with error: %@", error);
  NSString* errorString =
      NSLocalizedString(@"CantGetConfig Error DownloadError", nil);
  [self whineAtUser:@"CantGetConfig", errorString];
  return;
}

- (void)connection:(NSURLConnection*)connection
    didReceiveData:(NSData*)data {
  [configData_ appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection*)connection {
  NSString* configString =
      [[[NSString alloc] initWithData:configData_
                             encoding:NSUTF8StringEncoding] autorelease];
  if (!configString) {
    NSString* errorString =
        NSLocalizedString(@"CantGetConfig Error StringCreate", nil);
    [self whineAtUser:@"CantGetConfig", errorString];
    return;
  }

  [self parseConfig:configString];

  [self updateImageCombo];
  self.loadingConfigFinished = YES;
  [imageComboBox_ becomeFirstResponder];
}

- (void)parseConfig:(NSString*)configString {
  // Details of the config file format can be found in
  // src/platform/vboot_reference/user_tools/README_recovery.txt .

  NSArray* stanzas = [configString componentsSeparatedByString:@"\n\n"];

  // First stanza is autoupdate.
  if ([stanzas count]) {
    NSDictionary* autoupdate = [self parseStanza:[stanzas objectAtIndex:0]
                                   withArrayKeys:nil];

    NSString* configVersion = [autoupdate objectForKey:kConfigFileVersion];
    if (configVersion) {
      NSBundle* bundle = [NSBundle mainBundle];
      NSString* bundleVersion =
          [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

      NSComparisonResult order = [self compareVersion:configVersion
                                            toVersion:bundleVersion];

      if (order == NSOrderedDescending) {
        NSString* upgradeNote = [autoupdate objectForKey:kConfigUpgradeMessage];
        if (!upgradeNote)
          upgradeNote = @"";
        [self whineAtUser:@"VersionError",
                          bundleVersion, configVersion, upgradeNote];
        [[NSWorkspace sharedWorkspace] openURL:
            [NSURL URLWithString:kRecoveryToolURLString]];
        [NSApp terminate:self];
      }
    }
  }

  NSSet* requiredKeys =
      [NSSet setWithObjects:kConfigName, kConfigImageSize, kConfigFile,
                            kConfigSHA1, kConfigURL, nil];
  // Keys that may occur more than once.
  NSSet* arrayKeys = [NSSet setWithObjects:kConfigHWID, kConfigURL, nil];

  NSMutableDictionary* images = [[NSMutableDictionary alloc] init];
  // Skip the autoupdate stanza.
  for (NSUInteger stanza = 1; stanza < [stanzas count]; ++stanza) {
    NSDictionary* image = [self parseStanza:[stanzas objectAtIndex:stanza]
                              withArrayKeys:arrayKeys];

    // Verify and add.
    if ([image count]) {
      BOOL isValid = YES;
      NSArray* imageKeys = [image allKeys];
      for (NSString* key in requiredKeys) {
        if (![imageKeys containsObject:key]) {
          NSLog(@"Error: missing required key %@", key);
          isValid = NO;
        }
      }
      isValid &= [self isValidFilename:[image objectForKey:kConfigFile]];
      if (isValid) {
        for (NSString* hwid in [image objectForKey:kConfigHWID])
          [images setObject:image forKey:hwid];
      }
    }
  }
  images_ = images;
}

- (NSDictionary*)parseStanza:(NSString*)stanza
               withArrayKeys:(NSSet*)arrayKeys {
  NSMutableDictionary* result = [NSMutableDictionary dictionary];
  NSArray* lines = [stanza componentsSeparatedByString:@"\n"];

  for (NSString* line in lines) {
    line = [line stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];

    if ([line hasPrefix:@"#"])
      continue;

    NSArray* keyValue = [line componentsSeparatedByString:@"="];
    if ([keyValue count] != 2)
      continue;

    NSString* key = [keyValue objectAtIndex:0];
    NSString* value = [keyValue objectAtIndex:1];
    if (arrayKeys && [arrayKeys containsObject:key]) {
      NSMutableArray* items = [result objectForKey:key];
      if (!items) {
        items = [NSMutableArray array];
        [result setObject:items forKey:key];
      }
      [items addObject:value];
    } else {
      id oldObject = [result objectForKey:key];
      if (oldObject) {
        NSLog(@"Unexpectedly found two values for key %@; %@ and %@",
              key, oldObject, value);
      }
      [result setObject:value forKey:key];
    }
  }

  return result;
}

- (NSComparisonResult)compareVersion:(NSString*)left
                           toVersion:(NSString*)right {
  NSArray* leftComponents = [left componentsSeparatedByString:@"."];
  NSArray* rightComponents = [right componentsSeparatedByString:@"."];
  NSUInteger leftCount = [leftComponents count];
  NSUInteger rightCount = [rightComponents count];

  for (NSUInteger i = 0; i < std::max(leftCount, rightCount); ++i) {
    int leftComponent = 0, rightComponent = 0;
    if (i < leftCount)
      leftComponent = [[leftComponents objectAtIndex:i] intValue];
    if (i < rightCount)
      rightComponent = [[rightComponents objectAtIndex:i] intValue];

    if (leftComponent < rightComponent)
      return NSOrderedAscending;
    else if (leftComponent > rightComponent)
      return NSOrderedDescending;
  }

  return NSOrderedSame;
}

- (BOOL)isValidFilename:(NSString*)filename {
  // Paranoia!
  if (![filename length])
    return NO;
  if ([filename isEqualToString:@"."] || [filename isEqualToString:@".."])
    return NO;
  if ([filename hasPrefix:@"-"])  // will be interpreted as switch
    return NO;
  NSCharacterSet* shadyCharacters =
      [NSCharacterSet characterSetWithCharactersInString:@"/*?["];
  if ([filename rangeOfCharacterFromSet:shadyCharacters].location != NSNotFound)
    return NO;

  return YES;
}

- (BOOL)selectDeviceNextEnabled {
  return [images_ objectForKey:[imageComboBox_ stringValue]] != nil;
}

- (IBAction)selectLocalFile:(id)sender {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setAllowsMultipleSelection:NO];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setCanCreateDirectories:NO];
  [openPanel setCanChooseFiles:YES];
  if (!([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask)) {
    // Option key = allow burning of any file.
    [openPanel setDelegate:self];
  }

  [openPanel setPrompt:NSLocalizedString(@"SelectLocal Button", nil)];
  [openPanel setMessage:NSLocalizedString(@"SelectLocal File", nil)];

  NSInteger result = [openPanel runModal];
  if (result == NSFileHandlingPanelCancelButton)
    return;

  imagePath_ = [[[openPanel filenames] objectAtIndex:0] copy];
  isImageLocal_ = YES;
  NSDictionary* attrs =
      [[NSFileManager defaultManager] attributesOfItemAtPath:imagePath_
                                                       error:nil];
  imageSize_ = [[attrs objectForKey:NSFileSize] longLongValue];

  [self switchToTabAtIndex:kSelectUSBStickTab];
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString*)filename {
  BOOL isDirectory;
  BOOL fileExists =
      [[NSFileManager defaultManager] fileExistsAtPath:filename
                                           isDirectory:&isDirectory];
  if (!fileExists)  // huh?
    return NO;
  if (isDirectory)
    return ![[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename];

  if (![filename hasSuffix:@".bin"])
    return NO;

  const int kSectorSize = 512;
  std::vector<unsigned char> buffer(kSectorSize);
  int fd = HANDLE_EINTR(open([filename fileSystemRepresentation], O_RDONLY));
  if (fd < 0)
    return NO;

  ssize_t bytesRead = 0;
  while (bytesRead < kSectorSize) {
    ssize_t bytesJustRead = HANDLE_EINTR(read(fd,
                                              &buffer[0] + bytesRead,
                                              kSectorSize - bytesRead));
    if (bytesJustRead <= 0)
      break;
    bytesRead += bytesJustRead;
  }
  close(fd);

  if (bytesRead < kSectorSize)
    return NO;

  // For an MBR (or GPT) disk image, the first sector will end with 0x55AA.
  // http://en.wikipedia.org/wiki/Master_boot_record
  // http://en.wikipedia.org/wiki/GUID_Partition_Table#Legacy_MBR_.28LBA_0.29
  return buffer[kSectorSize - 2] == 0x55 && buffer[kSectorSize - 1] == 0xAA;
}

- (NSArray*)matchingImageHwids {
  NSArray* allHwids = [images_ allKeys];

  NSString* text = [imageComboBox_ stringValue];
  if (![text length])
    return allHwids;

  // First, do any hwids start with what was typed?
  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:@"SELF BEGINSWITH %@", text];
  NSArray* matches = [allHwids filteredArrayUsingPredicate:predicate];
  if ([matches count])
    return matches;

  // If not, try substring.
  predicate = [NSPredicate predicateWithFormat:@"SELF CONTAINS %@", text];
  return [allHwids filteredArrayUsingPredicate:predicate];
}

- (void)updateImageCombo {
  [imageComboBox_ removeAllItems];

  NSArray* matches = [[self matchingImageHwids] sortedArrayUsingSelector:
                         @selector(localizedCaseInsensitiveCompare:)];
  [imageComboBox_ addItemsWithObjectValues:matches];
}

- (void)imageComboTextChanged {
  // Automatically capitalize user input.
  NSString* text = [imageComboBox_ stringValue];
  [imageComboBox_ setStringValue:[text uppercaseString]];

  // Update list of matching HWIDs.
  [self updateImageCombo];

  // Pop up completions.
  if (!imageComboBoxPopped_) {
    [[imageComboBox_ cell] performSelector:@selector(popUp:)
                                withObject:nil
                                afterDelay:0];
  }
}

- (IBAction)imageWasSelected:(id)sender {
  NSArray* matches = [self matchingImageHwids];
  if ([matches count] == 1) {
    [imageComboBox_ setStringValue:[matches objectAtIndex:0]];
  }

  [self willChangeValueForKey:@"selectDeviceNextEnabled"];
  [self didChangeValueForKey:@"selectDeviceNextEnabled"];
}

- (BOOL)selectDeviceNext {
  if (![self selectDeviceNextEnabled])
    return NO;

  isImageLocal_ = NO;
  image_ = [images_ objectForKey:[imageComboBox_ stringValue]];
  imageSize_ = [[image_ objectForKey:kConfigImageSize] longLongValue];

  return YES;
}

#pragma mark Select USB Stick

- (BOOL)selectUSBStickNextEnabled {
  return [stickTable_ selectedRow] != -1;
}

- (BOOL)insertUSBStickHidden {
  return [sticks_ count] > 0;
}

- (void)diskAppeared:(DADiskRef)disk {
  if ([self isAcceptableMedia:disk]) {
    BOOL notify = [sticks_ count] == 0;
    if (notify)
      [self willChangeValueForKey:@"insertUSBStickHidden"];
    [sticks_ addObject:(id)disk];
    [stickTable_ reloadData];
    if (notify)
      [self didChangeValueForKey:@"insertUSBStickHidden"];
  }
}

- (void)diskDisappeared:(DADiskRef)disk {
  BOOL notify = [sticks_ count] == 1;
  if (notify)
    [self willChangeValueForKey:@"insertUSBStickHidden"];
  [sticks_ removeObject:(id)disk];
  [stickTable_ reloadData];
  if (notify)
    [self didChangeValueForKey:@"insertUSBStickHidden"];
}

- (void)diskPeek:(DADiskRef)disk {
  // Claim all USB sticks. This way if the user plugs in a blank stick to be
  // used, they won't get distracted by DiskArb complaining. (This won't prevent
  // it from mounting, though, if it has a mountable filesystem.)
  if ([self isAcceptableMedia:disk]) {
    DADiskClaim(disk,
                kDADiskClaimOptionDefault,
                ProtectiveDiskClaimRevoked,
                self,
                ProtectiveDiskClaimed,
                self);
  }
}

- (BOOL)isAcceptableMedia:(DADiskRef)disk {
  CFDictionaryRef info = DADiskCopyDescription(disk);

  // Be excruciatingly paranoid about what we consider to be a USB stick.
  NSNumber* internal = DictLookup(info, kDADiskDescriptionDeviceInternalKey);
  NSString* protocol = DictLookup(info, kDADiskDescriptionDeviceProtocolKey);
  NSString* ioRegPath = DictLookup(info, kDADiskDescriptionDevicePathKey);
  NSNumber* ejectable = DictLookup(info, kDADiskDescriptionMediaEjectableKey);
  NSNumber* removable = DictLookup(info, kDADiskDescriptionMediaRemovableKey);
  NSNumber* whole = DictLookup(info, kDADiskDescriptionMediaWholeKey);
  NSString* kind = DictLookup(info, kDADiskDescriptionMediaKindKey);

  // A drive is a USB stick iff:
  // - it is not internal
  // - it is attached to the USB bus
  // - it is ejectable (because it will be ejected after written to)
  // - it is removable
  // - it is the whole drive (although the use of
  //   kDADiskDescriptionMatchMediaWhole should have ensured this)
  // - it is of type IOMedia (external DVD drives and the like are IOCDMedia or
  //   IODVDMedia)

  BOOL isUSBStick = ![internal boolValue] &&
                     [protocol isEqualToString:@"USB"] &&
                     [ejectable boolValue] &&
                     [removable boolValue] &&
                     [whole boolValue] &&
                     [kind isEqualToString:@"IOMedia"];

  // A drive is an SD card iff:
  // - it is attached to the USB bus
  // - it is ejectable (because it will be ejected after written to)
  // - it is removable
  // - it is the whole drive (although the use of
  //   kDADiskDescriptionMatchMediaWhole should have ensured this)
  // - it is of type IOMedia (external DVD drives and the like are IOCDMedia or
  //   IODVDMedia)
  // - the IORegistry device path contains "AppleUSBCardReader"

  BOOL isSDCard = [protocol isEqualToString:@"USB"] &&
                  [ejectable boolValue] &&
                  [removable boolValue] &&
                  [whole boolValue] &&
                  [kind isEqualToString:@"IOMedia"] &&
                  [self hasIORegPathOfSDCard:ioRegPath];

  CFRelease(info);
  return isUSBStick || isSDCard;
}

- (BOOL)hasIORegPathOfSDCard:(NSString*)path {
  return [path rangeOfString:@"AppleUSBCardReader"].location != NSNotFound;
}

- (NSInteger)stickTableRowCount {
  return [sticks_ count];
}

- (id)stickTableObjectValueForRow:(NSInteger)row {
  DADiskRef disk = (DADiskRef)[sticks_ objectAtIndex:row];

  return [self descriptionForDisk:disk];
}

- (void)stickSelectionChanged {
  [self willChangeValueForKey:@"selectUSBStickNextEnabled"];
  [self didChangeValueForKey:@"selectUSBStickNextEnabled"];
}

- (IBAction)stickWasDoubleClicked:(id)sender {
  [self nextTab:sender];
}

- (BOOL)selectUSBStickNext {
  if ([stickTable_ selectedRow] == -1)
    return NO;

  DADiskRef disk = (DADiskRef)[sticks_ objectAtIndex:[stickTable_ selectedRow]];
  NSString* desc = [self descriptionForDisk:disk];
  CFDictionaryRef info = DADiskCopyDescription(disk);
  off_t diskSize =
      [DictLookup(info, kDADiskDescriptionMediaSizeKey) longLongValue];
  BOOL writable =
      [DictLookup(info, kDADiskDescriptionMediaWritableKey) boolValue];
  CFRelease(info);

  if (diskSize < imageSize_) {
    NSString* imageSizeString = SizeStringForValue(imageSize_);
    [self whineAtUser:@"TooSmallAlert", desc, imageSizeString];
    return NO;
  }

  if (!writable) {
    [self whineAtUser:@"NotWritable", desc];
    return NO;
  }

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setAlertStyle:NSCriticalAlertStyle];
  NSString* message = NSLocalizedString(@"WriteAlert Message", nil);
  message = [NSString stringWithFormat:message, desc];
  [alert setMessageText:message];
  [alert setInformativeText:NSLocalizedString(@"WriteAlert Informative", nil)];

  NSButton* button =
      [alert addButtonWithTitle:NSLocalizedString(@"WriteAlert OK", nil)];
  [button setKeyEquivalent:@""];
  button =
      [alert addButtonWithTitle:NSLocalizedString(@"WriteAlert Cancel", nil)];
  [button setKeyEquivalent:@"\e"];

  NSInteger result = [alert runModal];
  if (result == NSAlertSecondButtonReturn) {
    return NO;
  }

  CFRetain(disk);
  selectedStick_ = disk;

  [self startWriteImage];

  return YES;
}

- (NSString*)descriptionForDisk:(DADiskRef)disk {
  NSString* diskName;

  CFDictionaryRef info = DADiskCopyDescription(disk);
  NSString* ioRegPath = DictLookup(info, kDADiskDescriptionDevicePathKey);
  if ([self hasIORegPathOfSDCard:ioRegPath])
    diskName = NSLocalizedString(@"Label SD Card", nil);
  else {
    NSString* vendor = DictLookup(info, kDADiskDescriptionDeviceVendorKey);
    NSString* model = DictLookup(info, kDADiskDescriptionDeviceModelKey);
    diskName = [NSString stringWithFormat:@"%@ %@", vendor, model];
  }

  double size = [DictLookup(info, kDADiskDescriptionMediaSizeKey) doubleValue];

  NSString* desc = [NSString stringWithFormat:@"%@ (%@)",
      diskName, SizeStringForValue(size)];
  CFRelease(info);

  return desc;
}

#pragma mark Work

- (IBAction)knockItOff:(id)sender {
  // In response to this flag the active task is to stop what it's doing, and
  // call -cleanUp:.
  stopping_ = YES;
}

- (void)failWithInfo:(FailureInfo*)info {
  NSData* failureInfoData = [NSData dataWithBytes:info
                                           length:sizeof(FailureInfo)];
  [self performSelectorOnMainThread:@selector(cleanUp:)
                         withObject:failureInfoData
                      waitUntilDone:NO];
}

- (void)cleanUp:(NSData*)info {
  stopping_ = NO;

  [download_ autorelease];
  download_ = nil;

  if (downloadPath_) {
    [[NSFileManager defaultManager] removeItemAtPath:downloadPath_ error:nil];
    [downloadPath_ release];
    downloadPath_ = nil;
  }

  if (!isImageLocal_ && imagePath_) {
    [[NSFileManager defaultManager] removeItemAtPath:imagePath_ error:nil];
    [imagePath_ release];
    imagePath_ = nil;
  }

  if (info) {
    const FailureInfo* failureInfo = (const FailureInfo*)[info bytes];

    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    NSString* messageKey =
        [NSString stringWithUTF8String:failureInfo->failureStep];
    NSString* message = NSLocalizedString(messageKey, nil);
    if (failureInfo->errorDomain != FailureInfo::kNoErrorCodeAvailable) {
      const char* errorString;
      switch (failureInfo->errorDomain) {
        case FailureInfo::kErrnoError: {
          errorString = strerror(failureInfo->errorCode);
          break;
        }
        case FailureInfo::kReturnValueError: {
          NSString* error = NSLocalizedString(@"Failure Termination Status",
                                              nil);
          error = [NSString stringWithFormat:error, failureInfo->errorCode];
          errorString = [error UTF8String];
          break;
        }
        case FailureInfo::kInternalError: {
          NSString* errorKey = [NSString stringWithFormat:
              @"Failure Internal Error %d", failureInfo->errorCode];
          errorString = [NSLocalizedString(errorKey, nil) UTF8String];
          break;
        }
        default:
          assert(0);
      }
      message = [NSString stringWithFormat:message, errorString];
    }
    [alert setMessageText:message];

    [alert addButtonWithTitle:NSLocalizedString(@"Failure Try Again", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Failure Quit", nil)];

    NSInteger result = [alert runModal];
    if (result == NSAlertSecondButtonReturn) {
      [NSApp terminate:self];
      return;
    }
  }

  [self switchToTabAtIndex:kSelectUSBStickTab];
}

- (void)startWriteImage {
  SetDockTileProgress(kDockTileVisible, kDockTileIndeterminate, 0);
  if (isImageLocal_)
    [self unpackComplete];
  else
    [self doDownload];
}

- (void)doDownload {
  status_.statusText = "Status Downloading";
  status_.progressIndeterminate = YES;
  status_.progressBytes = 0;
  status_.attempt = urlIndex_;
  [self sendStatusUpdate];

  NSString* archiveURLString =
      [[image_ objectForKey:kConfigURL] objectAtIndex:urlIndex_];
  NSURL* archiveURL = [NSURL URLWithString:archiveURLString];
  NSURLRequest* request = [NSURLRequest requestWithURL:archiveURL];
  download_ = [[NSURLDownload alloc] initWithRequest:request
                                            delegate:self];
  NSString* tempDir = NSTemporaryDirectory();
  NSArray* components = [archiveURLString componentsSeparatedByString:@"/"];
  NSString* fileName = [components objectAtIndex:[components count] - 1];
  downloadPath_ = [[tempDir stringByAppendingPathComponent:fileName] retain];
  [download_ setDestination:downloadPath_
             allowOverwrite:YES];
}

- (void)download:(NSURLDownload*)download
    didReceiveResponse:(NSURLResponse*)response {
  long long expectedSize = [response expectedContentLength];
  if (expectedSize != NSURLResponseUnknownLength) {
    [progressIndicator_ setMinValue:0];
    [progressIndicator_ setMaxValue:expectedSize];
    status_.progressIndeterminate = NO;
  }

  if (stopping_) {
    [download_ cancel];
    [self performSelectorOnMainThread:@selector(cleanUp:)
                           withObject:nil
                        waitUntilDone:NO];
  }
}

- (void)download:(NSURLDownload*)download
    didReceiveDataOfLength:(NSUInteger)length {
  status_.progressBytes += length;
  [self sendStatusUpdate];

  if (stopping_) {
    [download_ cancel];
    [self performSelectorOnMainThread:@selector(cleanUp:)
                           withObject:nil
                        waitUntilDone:NO];
  }
}

- (BOOL)download:(NSURLDownload*)download
    shouldDecodeSourceDataOfMIMEType:(NSString*)encodingType {
  return NO;
}

- (void)downloadDidFinish:(NSURLDownload*)download {
  [self performSelectorInBackground:@selector(verify)
                         withObject:nil];
}

- (void)download:(NSURLDownload*)download
    didFailWithError:(NSError*)error {
  NSLog(@"Image download failed with error: %@", error);
  [download_ autorelease];
  download_ = nil;

  ++urlIndex_;
  if (urlIndex_ < [[image_ objectForKey:kConfigURL] count]) {
    [self doDownload];
  } else {
    FailureInfo failureInfo;
    failureInfo.failureStep = "Failure Downloading";
    failureInfo.errorDomain = FailureInfo::kNoErrorCodeAvailable;
    [self failWithInfo:&failureInfo];
    return;
  }
}

- (void)verify {
  ScopedNSAutoreleasePool poolOwner([[NSAutoreleasePool alloc] init]);
  status_.statusText = "Status Verifying Image";
  status_.progressIndeterminate = YES;
  status_.progressBytes = 0;
  status_.attempt = 0;
  [self sendStatusUpdate];

  NSTask* task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath: @"/usr/bin/openssl"];

  NSArray* arguments = [NSArray arrayWithObjects:@"sha1", downloadPath_, nil];
  [task setArguments:arguments];

  NSPipe* pipe = [NSPipe pipe];
  [task setStandardOutput:pipe];

  [task launch];
  while ([task isRunning] && !stopping_) {
    ScopedNSAutoreleasePool poolOwner([[NSAutoreleasePool alloc] init]);
    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:kNSTaskPollingInterval];
    [[NSRunLoop currentRunLoop] runUntilDate:date];
  }
  if (stopping_) {
    [task terminate];
    [self performSelectorOnMainThread:@selector(cleanUp:)
                           withObject:nil
                        waitUntilDone:NO];
    return;
  }
  int status = [task terminationStatus];

  if (status) {
    FailureInfo failureInfo;
    failureInfo.failureStep = "Failure Verifying Return Value";
    failureInfo.errorDomain = FailureInfo::kReturnValueError;
    failureInfo.errorCode = status;
    [self failWithInfo:&failureInfo];
    return;
  }

  NSData* output = [[pipe fileHandleForReading] readDataToEndOfFile];
  NSString* outputString =
      [[[NSString alloc] initWithData:output
                             encoding:NSUTF8StringEncoding] autorelease];
  outputString = [outputString stringByTrimmingCharactersInSet:
      [NSCharacterSet whitespaceAndNewlineCharacterSet]];
  NSArray* components = [outputString componentsSeparatedByString:@" "];
  NSString* actualSHA1 = [components objectAtIndex:[components count] - 1];
  if (![actualSHA1 isEqualToString:[image_ objectForKey:kConfigSHA1]]) {
    FailureInfo failureInfo;
    failureInfo.failureStep = "Failure Verifying SHA Mismatch";
    failureInfo.errorDomain = FailureInfo::kNoErrorCodeAvailable;
    [self failWithInfo:&failureInfo];
    return;
  }

  [self performSelectorOnMainThread:@selector(verifyComplete)
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)verifyComplete {
  [progressIndicator_ setMinValue:0];
  [progressIndicator_ setMaxValue:imageSize_];
  [self performSelectorInBackground:@selector(unpack)
                         withObject:nil];
}

- (void)unpack {
  ScopedNSAutoreleasePool poolOwner([[NSAutoreleasePool alloc] init]);
  status_.statusText = "Status Unpacking";
  status_.progressIndeterminate = YES;
  status_.progressBytes = 0;
  [self sendStatusUpdate];

  NSFileManager* fileManager = [[[NSFileManager alloc] init] autorelease];
  NSString* filename = [image_ objectForKey:kConfigFile];
  imagePath_ =
      [[NSTemporaryDirectory() stringByAppendingPathComponent:filename] retain];

  NSTask* task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath: @"/usr/bin/unzip"];
  NSArray* arguments = [NSArray arrayWithObjects:
      @"-o",  // overwrite existing
      @"-qq",  // quiet, no console spew
      @"-d", NSTemporaryDirectory(),  // target directory
      downloadPath_,
      filename,
      nil];
  [task setArguments:arguments];

  [task launch];
  while ([task isRunning] && !stopping_) {
    ScopedNSAutoreleasePool poolOwner([[NSAutoreleasePool alloc] init]);
    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:kNSTaskPollingInterval];
    [[NSRunLoop currentRunLoop] runUntilDate:date];

    NSDictionary* attrs =
        [fileManager attributesOfItemAtPath:imagePath_ error:nil];
    NSNumber* fileSize = [attrs objectForKey:NSFileSize];
    if (fileSize) {
      status_.progressBytes = [fileSize longLongValue];
      status_.progressIndeterminate = NO;
      [self sendStatusUpdate];
    }
  }
  if (stopping_) {
    [task terminate];
    [self performSelectorOnMainThread:@selector(cleanUp:)
                           withObject:nil
                        waitUntilDone:NO];
    return;
  }
  int status = [task terminationStatus];

  [fileManager removeItemAtPath:downloadPath_ error:nil];

  if (status) {
    FailureInfo failureInfo;
    failureInfo.failureStep = "Failure Unpacking";
    failureInfo.errorDomain = FailureInfo::kReturnValueError;
    failureInfo.errorCode = status;
    [self failWithInfo:&failureInfo];
    return;
  }

  NSDictionary* attrs =
      [fileManager attributesOfItemAtPath:imagePath_ error:nil];
  long long actualSize = [[attrs objectForKey:NSFileSize] longLongValue];
  if (actualSize != imageSize_) {
    // Paranoia? The actual size of the image is not the size claimed in the
    // config file, yet the zip file passed a SHA-1. Assume that the config file
    // got it wrong, but it won't hurt to complain.
    NSLog(@"File size doesn't match; actual size is %lld, while the "
           "configuration file claimed a size of %lld", actualSize, imageSize_);
    imageSize_ = actualSize;
  }

  [self performSelectorOnMainThread:@selector(unpackComplete)
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)unpackComplete {
  [progressIndicator_ setMinValue:0];
  [progressIndicator_ setMaxValue:imageSize_];
  [self performSelectorInBackground:@selector(writeImage)
                         withObject:nil];
}

namespace {

struct FileDelete {
  void operator()(NSString* path) const {
    if (path) {
      NSFileManager* fileManager = [[NSFileManager alloc] init];
      [fileManager removeItemAtPath:path error:nil];
      [fileManager release];
    }
  }
};
typedef scoped_ptr_malloc<NSString, FileDelete> FileOwner;

}  // namespace

- (void)writeImage {
  // Why a mutex/condition?
  //
  // DiskArb functions typically work this way: You call them (e.g.
  // DADiskClaim), and go on your way. Sometime in the future, on the main
  // thread, they call the indicated callback function specifying whether the
  // operation succeeded (if there were no dissenters) or failed. To avoid
  // having to break up the code into multiple functions, a mutex is used to
  // protect a condition, this thread sleeps on the condition, and when DiskArb
  // calls the callback the callback wakes up this thread which continues.
  //
  // Why a recursive mutex?
  //
  // Because the previous paragraph contains a lie. Most of the time DiskArb
  // does callbacks on the main thread. However, in certain error cases (e.g.
  // the user ripped the USB stick from the socket), calling the DiskArb
  // function results in an immediate callback on the thread that made the
  // DiskArb call. A recursive mutex is then needed to avoid deadlock.
  ScopedNSAutoreleasePool poolOwner([[NSAutoreleasePool alloc] init]);
  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_init(&mutexAttr);
  pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&diskArbMutex_, &mutexAttr);
  pthread_mutexattr_destroy(&mutexAttr);
  ScopedPThreadMutexOwner mutexOwner(&diskArbMutex_);
  pthread_cond_init(&diskArbCondition_, NULL);
  ScopedPThreadConditionOwner condOwner(&diskArbCondition_);
  pthread_mutex_lock(&diskArbMutex_);
  ScopedPThreadMutexLock mutexLock(&diskArbMutex_);
  FileOwner imageOwner(isImageLocal_ ? nil : imagePath_);
  FailureInfo failureInfo = { 0 };

  CFDictionaryRef info = DADiskCopyDescription(selectedStick_);
  off_t blockSize =
      [DictLookup(info, kDADiskDescriptionMediaBlockSizeKey) longLongValue];
  CFRelease(info);

  // Claim the disk.
  if (![claimedSticks_ containsObject:(id)selectedStick_]) {
    status_.statusText = "Status Claiming";
    status_.progressIndeterminate = YES;
    [self sendStatusUpdate];
    DADiskClaim(selectedStick_,
                kDADiskClaimOptionDefault,
                DiskClaimRevoked,
                self,
                DiskClaimed,
                self);
    diskArbSuccess_ = kDiskArbUnknown;
    while (diskArbSuccess_ == kDiskArbUnknown)
      pthread_cond_wait(&diskArbCondition_, &diskArbMutex_);
    if (diskArbSuccess_ == kDiskArbFailed) {
      failureInfo.failureStep = "Failure Claiming";
      failureInfo.errorDomain = FailureInfo::kNoErrorCodeAvailable;
      [self failWithInfo:&failureInfo];
      return;
    }
  } else {
    [claimedSticks_ removeObject:(id)selectedStick_];
  }
  ScopedDADiskClaim diskClaim(selectedStick_);

  // Unmount the disk.
  status_.statusText = "Status Unmounting";
  status_.progressIndeterminate = YES;
  [self sendStatusUpdate];
  diskArbSuccess_ = kDiskArbUnknown;
  DADiskUnmount(selectedStick_,
                kDADiskUnmountOptionForce | kDADiskUnmountOptionWhole,
                DiskUnmounted,
                self);
  while (diskArbSuccess_ == kDiskArbUnknown)
    pthread_cond_wait(&diskArbCondition_, &diskArbMutex_);
  if (diskArbSuccess_ == kDiskArbFailed) {
    failureInfo.failureStep = "Failure Unmounting";
    failureInfo.errorDomain = FailureInfo::kNoErrorCodeAvailable;
    [self failWithInfo:&failureInfo];
    return;
  }

  // Open the disk and image.
  int imagefd = HANDLE_EINTR(open([imagePath_ fileSystemRepresentation],
                                  O_RDONLY));
  if (imagefd < 0) {
    failureInfo.failureStep = "Failure Opening Source Image";
    failureInfo.errorDomain = FailureInfo::kErrnoError;
    failureInfo.errorCode = errno;
    [self failWithInfo:&failureInfo];
    return;
  }
  fcntl(imagefd, F_NOCACHE, 1);

  const char* bsdName = DADiskGetBSDName(selectedStick_);

  int flags = OPENDEV_PART;
  if (imageSize_ % blockSize) {
    flags |= OPENDEV_BLCK;
    NSLog(@"Warning: Size of image to be written (%lld) is not a multiple of "
          @"the device's block size (%lld); using block device which will be "
          @"significantly slower.",
          (long long)imageSize_, (long long)blockSize);
  }
  int devfd = HANDLE_EINTR(opendev((char*)bsdName,
                                   O_RDWR,
                                   flags,
                                   NULL));
  if (devfd < 0) {
    failureInfo.failureStep = "Failure Opening Destination Device";
    failureInfo.errorDomain = FailureInfo::kErrnoError;
    failureInfo.errorCode = errno;
    [self failWithInfo:&failureInfo];
    close(imagefd);
    return;
  }
  fcntl(devfd, F_NOCACHE, 1);

  // Prep timing.
  mach_timebase_info_data_t timebase;
  mach_timebase_info(&timebase);
  const uint64_t kProgressUpdateIntervalNanos = 100 * 1000 * 1000;  // = 100 ms
  uint64_t lastUpdateTime = 0;

  // Slam bits.
  status_.progressBytes = 0;
  status_.statusText = "Status Writing";
  status_.progressIndeterminate = NO;
  [self sendStatusUpdate];

  const int kBufferSize = 128 * 1024;
  std::vector<char> sourceBuffer(kBufferSize);
  while (status_.progressBytes < imageSize_ &&
         !failureInfo.failureStep &&
         !stopping_) {
    ssize_t bytesRead = HANDLE_EINTR(read(imagefd,
                                          &sourceBuffer[0],
                                          kBufferSize));
    if (bytesRead <= 0) {
      failureInfo.failureStep = "Failure Reading Source Image";
      if (bytesRead < 0) {
        failureInfo.errorDomain = FailureInfo::kErrnoError;
        failureInfo.errorCode = errno;
      } else {
        failureInfo.errorDomain = FailureInfo::kInternalError;
        failureInfo.errorCode = kInternalErrorUnexpectedEOF;
      }
      break;
    }

    ssize_t bytesWritten = 0;
    while (bytesRead > bytesWritten) {
      ssize_t bytesJustWritten =
          HANDLE_EINTR(write(devfd,
                             &sourceBuffer[0] + bytesWritten,
                             bytesRead - bytesWritten));
      if (bytesJustWritten <= 0) {
        failureInfo.failureStep = "Failure Writing Destination Device";
        if (bytesJustWritten < 0) {
          failureInfo.errorDomain = FailureInfo::kErrnoError;
          failureInfo.errorCode = errno;
        } else {
          failureInfo.errorDomain = FailureInfo::kInternalError;
          failureInfo.errorCode = kInternalErrorUnexpectedEOF;
        }
        break;
      }

      status_.progressBytes += bytesJustWritten;
      bytesWritten += bytesJustWritten;

      uint64_t now = mach_absolute_time();
      uint64_t difference =
          (now - lastUpdateTime) * timebase.numer / timebase.denom;
      if (difference > kProgressUpdateIntervalNanos) {
        [self sendStatusUpdate];
        lastUpdateTime = now;
      }
    }
  }

  // Verify bits.
  status_.progressBytes = 0;
  status_.statusText = "Status Verifying Media";
  status_.progressIndeterminate = NO;
  [self sendStatusUpdate];

  if (lseek(imagefd, 0, SEEK_SET) != 0) {
    failureInfo.failureStep = "Failure Resetting Source Image";
    failureInfo.errorDomain = FailureInfo::kErrnoError;
    failureInfo.errorCode = errno;
  }
  if (lseek(devfd, 0, SEEK_SET) != 0) {
    failureInfo.failureStep = "Failure Resetting Destination Device";
    failureInfo.errorDomain = FailureInfo::kErrnoError;
    failureInfo.errorCode = errno;
  }

  std::vector<char> destBuffer(kBufferSize);
  while (status_.progressBytes < imageSize_ &&
         !failureInfo.failureStep &&
         !stopping_) {
    ssize_t sourceBytesRead = HANDLE_EINTR(read(imagefd,
                                                &sourceBuffer[0],
                                                kBufferSize));
    if (sourceBytesRead <= 0) {
      failureInfo.failureStep = "Failure Reading Source Image";
      if (sourceBytesRead < 0) {
        failureInfo.errorDomain = FailureInfo::kErrnoError;
        failureInfo.errorCode = errno;
      } else {
        failureInfo.errorDomain = FailureInfo::kInternalError;
        failureInfo.errorCode = kInternalErrorUnexpectedEOF;
      }
      break;
    }

    ssize_t destBytesRead = 0;
    while (sourceBytesRead > destBytesRead) {
      ssize_t destBytesJustRead =
          HANDLE_EINTR(read(devfd,
                            &destBuffer[0] + destBytesRead,
                            sourceBytesRead - destBytesRead));
      if (destBytesJustRead <= 0) {
        failureInfo.failureStep = "Failure Reading Destination Device";
        if (destBytesJustRead < 0) {
          failureInfo.errorDomain = FailureInfo::kErrnoError;
          failureInfo.errorCode = errno;
        } else {
          failureInfo.errorDomain = FailureInfo::kInternalError;
          failureInfo.errorCode = kInternalErrorUnexpectedEOF;
        }
        break;
      }

      status_.progressBytes += destBytesJustRead;
      destBytesRead += destBytesJustRead;

      uint64_t now = mach_absolute_time();
      uint64_t difference =
          (now - lastUpdateTime) * timebase.numer / timebase.denom;
      if (difference > kProgressUpdateIntervalNanos) {
        [self sendStatusUpdate];
        lastUpdateTime = now;
      }
    }
  }

  // Close the disk and image.
  close(imagefd);
  close(devfd);

  // Eject the disk.
  status_.statusText = "Status Ejecting";
  status_.progressIndeterminate = YES;
  [self sendStatusUpdate];
  diskArbSuccess_ = kDiskArbUnknown;
  DADiskEject(selectedStick_,
              kDADiskEjectOptionDefault,
              DiskEjected,
              self);
  while (diskArbSuccess_ == kDiskArbUnknown)
    pthread_cond_wait(&diskArbCondition_, &diskArbMutex_);
  if (diskArbSuccess_ == kDiskArbFailed) {
    if (failureInfo.failureStep) {
      // Failure to eject is probably not the real problem; ignore it.
    } else {
      failureInfo.failureStep = "Failure Ejecting";
      failureInfo.errorDomain = FailureInfo::kNoErrorCodeAvailable;
      [self failWithInfo:&failureInfo];
      return;
    }
  }

  // Unclaim the disk.
  diskClaim.reset();

  if (stopping_) {
    [self performSelectorOnMainThread:@selector(cleanUp:)
                           withObject:nil
                        waitUntilDone:NO];
    return;
  }

  if (status_.progressBytes < imageSize_) {
    [self failWithInfo:&failureInfo];
    return;
  }

  status_.statusText = "Status Done";
  status_.progressIndeterminate = NO;
  [self sendStatusUpdate];

  // Let the user continue in the UI.
  [self performSelectorOnMainThread:@selector(writeImageComplete)
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)writeImageComplete {
  [self nextTab:self];
}

- (void)sendStatusUpdate {
  NSData* statusData = [[NSData alloc] initWithBytes:&status_
                                              length:sizeof(status_)];
  [self performSelectorOnMainThread:@selector(updateStatus:)
                         withObject:statusData
                      waitUntilDone:NO];
  [statusData release];
}

- (void)updateStatus:(NSData*)value {
  const Status* statusData = (const Status*)[value bytes];

  NSString* statusLine = NSLocalizedString(
      [NSString stringWithUTF8String:statusData->statusText], nil);
  if (statusData->attempt) {
    NSString* addition =
        [NSString stringWithFormat:NSLocalizedString(@"StatusAdd Attempt", nil),
                                   statusData->attempt + 1];
    statusLine = [statusLine stringByAppendingString:addition];
  }
  [statusLine_ setStringValue:statusLine];

  BOOL currentlyIndeterminate = [progressIndicator_ isIndeterminate];
  if ((bool)currentlyIndeterminate != (bool)statusData->progressIndeterminate) {
    [progressIndicator_ setIndeterminate:statusData->progressIndeterminate];
    if (statusData->progressIndeterminate)
      [progressIndicator_ startAnimation:self];
  }
  [progressIndicator_ setDoubleValue:statusData->progressBytes];

  double progressValue = 0.0;
  if ([progressIndicator_ maxValue])
    progressValue = statusData->progressBytes / [progressIndicator_ maxValue];
  SetDockTileProgress(kDockTileVisible,
                      statusData->progressIndeterminate ? kDockTileIndeterminate
                                                        : kDockTileDeterminate,
                      progressValue);
}

- (BOOL)workNext {
  SetDockTileProgress(kDockTileInvisible, kDockTileIndeterminate, 0);

  return YES;
}

#pragma mark Done

@end
