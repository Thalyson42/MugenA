/* Android port: JNI entry points of libmugen.so.
 *
 * P1: the library only proved that it builds, links its dependencies and loads on the device.
 * P3a: it also links the platform layer (src/platform) and runs it from the app: initPlatform (data directory,
 * stdout/stderr to logcat, error dialog hook), platformSelfTest and testError. The engine itself joins in P3b. */
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include <png.h>
#include <lua.h>
#include <zlib.h>

#include "platform/platform.h"

#define TAG "mugen"

extern int P3a_SelfTest(char *report, size_t cap, const char *altDir);

static JavaVM *g_vm;
static jclass g_nativeClass;        /* global reference to io.github.thalyson42.mugen.Native */
static jmethodID g_showError;       /* static void showError(String title, String text) */

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

/*
 * NewStringUTF wants (modified) UTF-8 and CheckJNI aborts the process on anything else. The engine's texts carry
 * the bytes of the game's files (often Shift-JIS), so copy keeping ASCII and well-formed 2- and 3-byte sequences,
 * and turn every other byte into '?'.
 */
static void SanitizeUtf8(const char *in, char *out, size_t size)
{
    const unsigned char *s = (const unsigned char *)in;
    size_t n = 0;

    while (*s != '\0' && n + 4 < size) {
        if (*s < 0x80) {
            out[n++] = (char)*s++;
        } else if (s[0] >= 0xC2 && s[0] <= 0xDF && (s[1] & 0xC0) == 0x80) {
            out[n++] = (char)s[0];
            out[n++] = (char)s[1];
            s += 2;
        } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
                   !(s[0] == 0xE0 && s[1] < 0xA0) && !(s[0] == 0xED && s[1] >= 0xA0)) {
            out[n++] = (char)s[0];
            out[n++] = (char)s[1];
            out[n++] = (char)s[2];
            s += 3;
        } else {
            out[n++] = '?';
            ++s;
        }
    }
    out[n] = '\0';
}

/* The error hook (Mugen_SetErrorHandler): shows the text in a dialog of the app and returns when it is dismissed.
 * Runs on the engine's thread, never on the UI thread (Native.showError waits for the dialog). */
static void ErrorHandler(const char *title, const char *text)
{
    JNIEnv *env = NULL;
    int attached = 0;
    char t[256], x[0x4100];
    jstring jt, jx;

    if (g_vm == NULL || g_nativeClass == NULL || g_showError == NULL) {
        return;
    }
    if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) != JNI_OK) {
            return;
        }
        attached = 1;
    }
    SanitizeUtf8(title, t, sizeof t);
    SanitizeUtf8(text, x, sizeof x);
    jt = (*env)->NewStringUTF(env, t);
    jx = (*env)->NewStringUTF(env, x);
    if (jt != NULL && jx != NULL) {
        (*env)->CallStaticVoidMethod(env, g_nativeClass, g_showError, jt, jx);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
    }
    if (jt != NULL) {
        (*env)->DeleteLocalRef(env, jt);
    }
    if (jx != NULL) {
        (*env)->DeleteLocalRef(env, jx);
    }
    if (attached) {
        (*g_vm)->DetachCurrentThread(g_vm);
    }
}

/* D-005: dataDir is the app's own folder (Android/data/<package>/files, or the internal files dir as a fallback).
 * It becomes the current directory, so the engine's relative paths and mugen.log land in it. */
JNIEXPORT jboolean JNICALL
Java_io_github_thalyson42_mugen_Native_initPlatform(JNIEnv *env, jclass cls, jstring dataDir)
{
    const char *dir = (*env)->GetStringUTFChars(env, dataDir, NULL);
    int ok;
    (void)cls;

    if (dir == NULL) {
        return JNI_FALSE;
    }
    ok = Mugen_SetDataDir(dir);
    __android_log_print(ok ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG, "data directory %s: %s", dir,
                        ok ? "ok" : "chdir failed");
    (*env)->ReleaseStringUTFChars(env, dataDir, dir);
    if (ok) {
        Mugen_StdioToLog(NULL);                 /* printf/fprintf(stderr) of the engine to logcat; only the first call acts */
        Mugen_SetErrorHandler(ErrorHandler);
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_io_github_thalyson42_mugen_Native_platformSelfTest(JNIEnv *env, jclass cls, jstring altDir)
{
    static char report[16384];
    char safe[sizeof report];
    const char *alt = altDir != NULL ? (*env)->GetStringUTFChars(env, altDir, NULL) : NULL;
    (void)cls;

    P3a_SelfTest(report, sizeof report, alt);
    if (alt != NULL) {
        (*env)->ReleaseStringUTFChars(env, altDir, alt);
    }
    SanitizeUtf8(report, safe, sizeof safe);   /* file names of the game folder may not be UTF-8 */
    return (*env)->NewStringUTF(env, safe);
}

/* Shows the error dialog the way Error_Exit would (called from a background thread by the app's test button). */
JNIEXPORT void JNICALL
Java_io_github_thalyson42_mugen_Native_testError(JNIEnv *env, jclass cls)
{
    (void)env;
    (void)cls;
    Mugen_ShowError("M.U.G.E.N Error", "Error detected.\n\nThis is a test of the error screen (P3a).\n"
                    "Non-UTF-8 bytes are made visible: \x83\x65\x83\x58\x83\x67\n\nClipboard tail:\nCan't find file chars\\kfm\\kfm.def\n");
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    (void)reserved;

    g_vm = vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        jclass c = (*env)->FindClass(env, "io/github/thalyson42/mugen/Native");

        if (c != NULL) {
            g_nativeClass = (*env)->NewGlobalRef(env, c);
            g_showError = (*env)->GetStaticMethodID(env, c, "showError", "(Ljava/lang/String;Ljava/lang/String;)V");
            (*env)->DeleteLocalRef(env, c);
        }
        if (g_showError == NULL) {
            (*env)->ExceptionClear(env);
        }
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "libmugen.so loaded (P3a)");
    return JNI_VERSION_1_6;
}
