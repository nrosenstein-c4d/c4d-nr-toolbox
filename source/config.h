// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <c4d.h>
#include <dlib/config_reader.h>

namespace nr {
namespace config {

  /**
   * Parse the configuration file and initialize the global config reader.
   */
  bool init_config(Filename const& filename, String* error);

  /**
   * Returns true if the feature with the specified name is enabled, false
   * otherwise.
   */
  bool check_feature(String const& feature_name);

  /**
   * Sets the status of a feature.
   */
  void enable_feature(String const& feature_name, Bool enabled);

  /**
   * Writes the configuration back to the user preferences.
   */
  bool save_config(Filename const* fn=nullptr);

}} // namespace nr::config
