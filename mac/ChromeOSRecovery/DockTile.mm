// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DockTile.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <algorithm>

static const double kSystemProgressAnimationInterval = 1.0 / 30.0;
static const int kFrameSkip = 3;  // Try to not waste too much CPU.
static const double kProgressBarHeight = 20.0 / 128.0;
static const double kProgressBarOffset = 20.0 / 128.0;

// A view that draws the dock tile.
@interface DockTileView : NSView {
 @private
  BOOL hidden_;
  BOOL indeterminate_;
  double progress_;

  NSTimer* timer_;  // Animation timer. Weak, owned by the run loop.
  UInt8 phase_;
}

// Indicates whether the progress bar should be visible or not.
- (void)setHidden:(BOOL)hidden;

// Indicates whether the progress bar should be in an indeterminate state or
// not.
- (void)setIndeterminate:(BOOL)indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
- (void)setProgress:(double)progress;

@end

@interface DockTileView (Private)

- (void)animate:(NSTimer*)timer;

@end

@implementation DockTileView

- (void)dealloc {
  [timer_ invalidate];

  [super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect {
  // Not -[NSApplication applicationIconImage]; that fails to return a pasted
  // custom icon.
  NSString* appPath = [[NSBundle mainBundle] bundlePath];
  NSImage* appIcon = [[NSWorkspace sharedWorkspace] iconForFile:appPath];
  [appIcon drawInRect:[self bounds]
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0];

  if (hidden_)
    return;

  CGRect rect = NSRectToCGRect([self bounds]);
  rect.origin.y = kProgressBarOffset * rect.size.height;
  rect.size.height = kProgressBarHeight * rect.size.height;

  HIThemeTrackDrawInfo trackInfo;
  trackInfo.version = 0;
  trackInfo.kind =
      indeterminate_ ? kThemeIndeterminateBarLarge : kThemeProgressBarLarge;
  trackInfo.bounds = rect;
  trackInfo.min = 0;
  trackInfo.max = std::numeric_limits<SInt32>::max();
  trackInfo.value = progress_ * std::numeric_limits<SInt32>::max();
  trackInfo.attributes = kThemeTrackHorizontal;
  trackInfo.enableState = kThemeTrackActive;
  trackInfo.trackInfo.progress.phase = phase_;

  CGContextRef context =
      (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);

  HIThemeDrawTrack(&trackInfo,
                   0,
                   context,
                   kHIThemeOrientationInverted);
}

- (void)setHidden:(BOOL)hidden {
  hidden_ = hidden;

  if (hidden) {
    if (timer_) {
      [timer_ invalidate];
      timer_ = nil;
      [self animate:nil];
    }
  } else if (!timer_) {
    timer_ =
        [NSTimer scheduledTimerWithTimeInterval:kSystemProgressAnimationInterval
                                                    * kFrameSkip
                                         target:self
                                       selector:@selector(animate:)
                                       userInfo:nil
                                        repeats:YES];
  }
}

- (void)setIndeterminate:(BOOL)indeterminate {
  indeterminate_ = indeterminate;
}

- (void)setProgress:(double)progress {
  progress_ = progress;
}

- (void)animate:(NSTimer*)timer {
  phase_ += kFrameSkip;
  [[[NSApplication sharedApplication] dockTile] display];
}

@end

void SetDockTileProgress(DockTileVisibility visibility,
                         DockTileProgressType type,
                         double value) {
  NSDockTile* dock_tile = [[NSApplication sharedApplication] dockTile];

  DockTileView* dock_tile_view =
      static_cast<DockTileView*>([dock_tile contentView]);
  if (!dock_tile_view ||
      ![dock_tile_view isKindOfClass:[DockTileView class]]) {
    dock_tile_view = [[[DockTileView alloc] init] autorelease];
    [dock_tile setContentView:dock_tile_view];
  }

  [dock_tile_view setHidden:visibility == kDockTileInvisible];
  [dock_tile_view setIndeterminate:type == kDockTileIndeterminate];
  [dock_tile_view setProgress:std::min(1.0, std::max(0.0, value))];
}
