
#pragma once
#include <cstdint>
#include <string>

namespace fs {

  /*!
   * Read the last modification time of a file. Returns 0 if the
   * time could not be read (eg. if the file does not exist).
   */
  // @{
  uint64_t getmtime(char const* file);
  inline uint64_t getmtime(std::string const& file) { return getmtime(file.c_str()); }
  // @}

} // namespace fs
