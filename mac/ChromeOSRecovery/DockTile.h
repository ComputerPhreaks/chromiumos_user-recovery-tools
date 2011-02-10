// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOCKTILE_H_
#define DOCKTILE_H_
#pragma once

enum DockTileVisibility {
  kDockTileInvisible,
  kDockTileVisible
};

enum DockTileProgressType {
  kDockTileIndeterminate,
  kDockTileDeterminate
};

void SetDockTileProgress(DockTileVisibility visibility,
                         DockTileProgressType type,
                         double value);

#endif  // DOCKTILE_H_
