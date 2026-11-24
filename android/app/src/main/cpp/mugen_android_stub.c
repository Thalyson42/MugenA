/* Android port, phase P1: the empty engine library.
 *
 * Only proves that libmugen.so builds, links its dependencies and loads on the device. The engine itself
 * (src/*.c) is added to this library in the later phases. */
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include <png.h>
#include <lua.h>
#include <zlib.h>

#define TAG "mugen"

/* Versions of the statically linked libraries, one per line, shown by the diagnostic app. */
JNIEXPORT jstring JNICALL
Java_io_github_thalyson42_mugen_Native_libraryVersions(JNIEnv *env, jclass cls)
{
    char buf[256];
    (void)cls;
    snprintf(buf, sizeof buf, "%s\nlibpng %s\nzlib %s\n%zu-bit pointers",
             LUA_RELEASE, PNG_LIBPNG_VER_STRING, zlibVersion(), sizeof(void *) * 8);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void)vm; (void)reserved;
    __android_log_print(ANDROID_LOG_INFO, TAG, "libmugen.so loaded (P1 stub)");
    return JNI_VERSION_1_6;
}
