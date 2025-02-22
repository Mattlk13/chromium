// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_

#include <string>

#include "base/location.h"

namespace syncer {

// A minimal error object for use by USS model type code.
class ModelError {
 public:
  // Creates an un-set error object (indicating an operation was successful).
  ModelError();

  // Creates a set error object with the given location and message.
  ModelError(const tracked_objects::Location& location,
             const std::string& message);

  ~ModelError();

  // Whether this object represents an actual error.
  bool IsSet() const;

  // The location of the error this object represents. Can only be called if the
  // error is set.
  const tracked_objects::Location& location() const;

  // The message explaining the error this object represents. Can only be called
  // if the error is set.
  const std::string& message() const;

 private:
  bool is_set_;
  tracked_objects::Location location_;
  std::string message_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
