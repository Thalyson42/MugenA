/* P2 STUB, Android build only: forwards to the libresample header of the SDK (a Windows SDK folder that also
 * holds pthreads-win32 headers, so the folder itself cannot be on the Android include path). The header marks
 * its functions __declspec(dllimport); that is dropped here. Temporary until libresample is built from source
 * for Android (D-003, P3/P4). See D-013. */
#define __declspec(x)
#include "../../../third_party/sdk/include/libresample.h"
#undef __declspec
