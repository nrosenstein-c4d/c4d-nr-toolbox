#include "fs.hpp"

#if defined(_WIN32)
  #include <Windows.h>

  uint64_t fs::getmtime(char const* file)
  {
    FILETIME mt;
    DWORD const flags = FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ;
    HANDLE const fp = CreateFileA(
      file, GENERIC_READ, flags, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (fp && GetFileTime(fp, nullptr, nullptr, &mt)) {
      // Convert the FILETIME structure to a 64-Bit number.
      return static_cast<uint64_t>(mt.dwHighDateTime) << 32 | mt.dwLowDateTime;
    }
    return 0;
  }

#elif defined(__APPLE__) || defined(__linux__) || defined(__GNUC__)
  #include <sys/stat.h>

  uint64_t fs::getmtime(char const* file)
  {
    struct stat buf = {0};
    stat(file, &buf);
    return buf.st_mtime;
  }

#else
  #error "Unknown compiler or platform"
#endif
