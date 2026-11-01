/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "platform/OpenSSLCompat.h"

#include "base/Log.h"

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <mutex>
#include <openssl/provider.h>
#endif

#if defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <string>
#include <sys/stat.h>
#endif

#if defined(Q_OS_UNIX)
#include <cstdlib>
#endif

namespace deskflow::platform {

void initializeOpenSSL()
{
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
  static std::once_flag initFlag;
  std::call_once(initFlag, []() {
#if defined(Q_OS_MAC)
    // On macOS, if we are running from a bundle, find the ossl-modules directory
    // This MUST be done before loading any providers.
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
      CFURLRef frameworkURL = CFBundleCopyPrivateFrameworksURL(mainBundle);
      if (frameworkURL) {
        char path[1024];
        if (CFURLGetFileSystemRepresentation(frameworkURL, true, (UInt8 *)path, sizeof(path))) {
          std::string modulesPath = std::string(path) + "/ossl-modules";
          struct stat st;
          if (stat(modulesPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            setenv("OPENSSL_MODULES", modulesPath.c_str(), 1);
          } else {
            // Not found in bundle, will use system default
          }
        }
        CFRelease(frameworkURL);
      }
    }
#endif

    // Load default provider (for standard algos)
    OSSL_PROVIDER *def = OSSL_PROVIDER_load(nullptr, "default");
    if (!def) {
      // Don't return, try legacy as well just in case
    }

    // Load legacy provider (sometimes needed for older curves/hashes)
    OSSL_PROVIDER *leg = OSSL_PROVIDER_load(nullptr, "legacy");
    if (!leg) {
    }
  });
#else
  // On Windows/Other non-OpenSSL 3 setups, this is a no-op or handled by the library init
#endif
}

} // namespace deskflow::platform
