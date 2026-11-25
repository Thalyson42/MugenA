/*
 * Case-insensitive path resolution and the files side of the POSIX / Android platform layer (phase P3a, R5/D-005).
 *
 * M.U.G.E.N content was made for Windows: names differ in case from the references to them ("Chars\KFM\kfm.DEF")
 * and paths use '\'. Android file systems are case-sensitive, so every file the engine opens goes through here:
 *   1. '\' becomes '/', "//" collapses, a trailing '/' is dropped.
 *   2. The path is tried as it is (one stat; the common case).
 *   3. Otherwise it is walked component by component: a component that does not exist with that exact spelling is
 *      looked up in its directory ignoring ASCII case, and replaced by the real name.
 *
 * Rules (documented because they are decisions, see D-005 in docs/android-port/03-decisoes.md):
 *   - Only ASCII letters are folded. Windows also folds other letters of the active code page; names with
 *     non-ASCII letters that differ in case between reference and file will not match.
 *   - When two entries differ only in case, the exact spelling wins; otherwise the one that sorts first (strcmp).
 *   - "." and ".." are never looked up. A component that cannot be found is kept as written, together with the
 *     rest of the path, so that creating a file in an existing (case-corrected) directory works.
 *   - Directory listings are cached (64 directories) and revalidated by the directory's inode and mtime on every
 *     use, so files created by anything else are seen. Files created through Mugen_fopen invalidate their directory.
 * Thread-safe (the BGM thread opens files).
 *
 * This file is plain POSIX and also builds on the host for the tests in tests/platform.
 */
#include "platform.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#undef fopen   /* platform_posix.h redirects fopen to Mugen_fopen, which is implemented with the real one */

#define PATH_BUF 4096
#define CACHE_DIRS 64
#define CACHE_MAX_NAMES 65536

typedef struct DirCache {
    char *path;              /* the directory as it was passed to Lookup (exact spelling); NULL = slot unused */
    ino_t ino;
    long long mtime_sec;
    long mtime_nsec;
    off_t size;
    int count;
    char **names;            /* count entries, each points into block */
    char *block;
} DirCache;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static DirCache g_cache[CACHE_DIRS];
static int g_nextVictim;
static MugenPathStats g_stats;

static int FoldChar(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int EqualNoCase(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (FoldChar((unsigned char)*a) != FoldChar((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static void FreeEntry(DirCache *e)
{
    free(e->path);
    free(e->names);
    free(e->block);
    memset(e, 0, sizeof(*e));
}

/* Reads the names of dir into e. Returns 0 on failure (e is left empty but keeps its path). */
static int LoadEntry(DirCache *e, const char *dir, const struct stat *st)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    size_t cap = 0, used = 0, ncap = 0;
    char *block = NULL;
    char **names = NULL;
    int n = 0, i;

    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL && n < CACHE_MAX_NAMES) {
        size_t len = strlen(de->d_name) + 1;

        if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
            continue;
        }
        if (used + len > cap) {
            char *nb;
            cap = (cap == 0 ? 4096 : cap * 2);
            while (used + len > cap) {
                cap *= 2;
            }
            nb = (char *)realloc(block, cap);
            if (nb == NULL) {
                n = -1;
                break;
            }
            block = nb;
        }
        memcpy(block + used, de->d_name, len);
        used += len;
        ++n;
    }
    closedir(d);
    if (n < 0) {
        free(block);
        return 0;
    }
    ncap = (size_t)n * sizeof(char *);
    names = (char **)malloc(ncap == 0 ? 1 : ncap);
    if (names == NULL) {
        free(block);
        return 0;
    }
    {
        size_t off = 0;
        for (i = 0; i < n; ++i) {
            names[i] = block + off;
            off += strlen(block + off) + 1;
        }
    }
    free(e->names);
    free(e->block);
    e->names = names;
    e->block = block;
    e->count = n;
    e->ino = st->st_ino;
    e->mtime_sec = (long long)st->st_mtim.tv_sec;
    e->mtime_nsec = (long)st->st_mtim.tv_nsec;
    e->size = st->st_size;
    ++g_stats.reloads;
    return 1;
}

/*
 * Looks name up in dir ignoring ASCII case. Returns 1 and copies the real name to out when found. Caller holds the lock.
 */
static int Lookup(const char *dir, const char *name, char *out, size_t out_size)
{
    struct stat st;
    DirCache *e = NULL;
    int i, best = -1;

    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return 0;
    }
    for (i = 0; i < CACHE_DIRS; ++i) {
        if (g_cache[i].path != NULL && strcmp(g_cache[i].path, dir) == 0) {
            e = &g_cache[i];
            break;
        }
    }
    if (e == NULL) {
        e = &g_cache[g_nextVictim];
        g_nextVictim = (g_nextVictim + 1) % CACHE_DIRS;
        FreeEntry(e);
        e->path = strdup(dir);
        if (e->path == NULL) {
            return 0;
        }
        if (!LoadEntry(e, dir, &st)) {
            FreeEntry(e);
            return 0;
        }
        ++g_stats.misses;
    } else if (e->ino != st.st_ino || e->mtime_sec != (long long)st.st_mtim.tv_sec ||
               e->mtime_nsec != (long)st.st_mtim.tv_nsec || e->size != st.st_size) {
        if (!LoadEntry(e, dir, &st)) {
            FreeEntry(e);
            return 0;
        }
        ++g_stats.misses;
    } else {
        ++g_stats.hits;
    }
    for (i = 0; i < e->count; ++i) {
        if (strcmp(e->names[i], name) == 0) {
            best = i;
            break;
        }
        if (EqualNoCase(e->names[i], name) && (best < 0 || strcmp(e->names[i], e->names[best]) < 0)) {
            best = i;
        }
    }
    if (best < 0 || strlen(e->names[best]) + 1 > out_size) {
        return 0;
    }
    strcpy(out, e->names[best]);
    return 1;
}

static void InvalidateDir(const char *dir)
{
    int i;

    pthread_mutex_lock(&g_lock);
    for (i = 0; i < CACHE_DIRS; ++i) {
        if (g_cache[i].path != NULL && strcmp(g_cache[i].path, dir) == 0) {
            FreeEntry(&g_cache[i]);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

/* '\' -> '/', collapses repeated separators, drops a trailing one (but keeps "/"). Returns 0 if it does not fit. */
static int Normalize(const char *in, char *out, size_t size)
{
    size_t n = 0;

    for (; *in != '\0'; ++in) {
        char c = (*in == '\\') ? '/' : *in;

        if (c == '/' && n > 0 && out[n - 1] == '/') {
            continue;
        }
        if (n + 1 >= size) {
            return 0;
        }
        out[n++] = c;
    }
    if (n > 1 && out[n - 1] == '/') {
        --n;
    }
    out[n] = '\0';
    return 1;
}

int Mugen_ResolvePath(const char *path, char *out, size_t size)
{
    char norm[PATH_BUF];
    char result[PATH_BUF];
    char real[256];
    struct stat st;
    size_t rlen;
    const char *p;
    int found = 1;

    if (path == NULL || out == NULL || size == 0) {
        return -1;
    }
    if (!Normalize(path, norm, sizeof(norm))) {
        return -1;
    }
    if (norm[0] == '\0' || stat(norm, &st) == 0) {
        if (strlen(norm) + 1 > size) {
            return -1;
        }
        strcpy(out, norm);
        return norm[0] != '\0';
    }

    result[0] = '\0';
    rlen = 0;
    p = norm;
    if (*p == '/') {
        strcpy(result, "/");
        rlen = 1;
        ++p;
    }
    while (*p != '\0') {
        const char *end = strchr(p, '/');
        size_t clen = (end != NULL) ? (size_t)(end - p) : strlen(p);
        char comp[256];
        const char *use = comp;

        if (clen >= sizeof(comp)) {
            found = 0;      /* longer than any file name: keep the rest as written */
            use = NULL;
        } else {
            memcpy(comp, p, clen);
            comp[clen] = '\0';
        }
        if (use != NULL && found) {
            int is_dots = (strcmp(comp, ".") == 0 || strcmp(comp, "..") == 0);
            char cand[PATH_BUF];
            int ok = 1;

            if (!is_dots) {
                if (rlen + (rlen > 0 && result[rlen - 1] != '/' ? 1 : 0) + clen + 1 > sizeof(cand)) {
                    return -1;
                }
                snprintf(cand, sizeof(cand), "%s%s%s", result,
                         (rlen > 0 && result[rlen - 1] != '/') ? "/" : "", comp);
                if (stat(cand, &st) != 0) {
                    pthread_mutex_lock(&g_lock);
                    ok = Lookup(rlen == 0 ? "." : result, comp, real, sizeof(real));
                    pthread_mutex_unlock(&g_lock);
                    if (ok) {
                        use = real;
                    }
                }
            }
            if (!ok) {
                found = 0;
                use = comp;
            }
        } else {
            found = 0;
            use = NULL;
        }
        if (use == NULL) {
            /* append everything left as written */
            size_t left = strlen(p);

            if (rlen > 0 && result[rlen - 1] != '/') {
                result[rlen++] = '/';
            }
            if (rlen + left + 1 > sizeof(result)) {
                return -1;
            }
            memcpy(result + rlen, p, left + 1);
            rlen += left;
            break;
        }
        {
            size_t ulen = strlen(use);

            if (rlen + 1 + ulen + 1 > sizeof(result)) {
                return -1;
            }
            if (rlen > 0 && result[rlen - 1] != '/') {
                result[rlen++] = '/';
            }
            memcpy(result + rlen, use, ulen + 1);
            rlen += ulen;
        }
        p += clen;
        if (*p == '/') {
            ++p;
        }
    }
    if (rlen + 1 > size) {
        return -1;
    }
    strcpy(out, result);
    return found && stat(out, &st) == 0;
}

const char *Mugen_ResolvePathTmp(const char *path)
{
    static _Thread_local char buf[PATH_BUF];

    if (path == NULL) {
        return NULL;
    }
    if (Mugen_ResolvePath(path, buf, sizeof(buf)) < 0) {
        return path;
    }
    return buf;
}

FILE *Mugen_fopen(const char *path, const char *mode)
{
    char resolved[PATH_BUF];
    int creates = (mode != NULL && (mode[0] != 'r' || strchr(mode, '+') != NULL));
    FILE *fp;

    if (path == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (Mugen_ResolvePath(path, resolved, sizeof(resolved)) < 0) {
        return fopen(path, mode);       /* too long to resolve: let the C library report it */
    }
    fp = fopen(resolved, mode);
    if (fp != NULL && creates) {
        char dir[PATH_BUF];
        char *slash;

        strcpy(dir, resolved);
        slash = strrchr(dir, '/');
        if (slash == NULL) {
            strcpy(dir, ".");
        } else if (slash == dir) {
            dir[1] = '\0';
        } else {
            *slash = '\0';
        }
        InvalidateDir(dir);
    }
    return fp;
}

void Mugen_PathCacheClear(void)
{
    int i;

    pthread_mutex_lock(&g_lock);
    for (i = 0; i < CACHE_DIRS; ++i) {
        FreeEntry(&g_cache[i]);
    }
    memset(&g_stats, 0, sizeof(g_stats));
    pthread_mutex_unlock(&g_lock);
}

void Mugen_PathCacheStats(MugenPathStats *out)
{
    pthread_mutex_lock(&g_lock);
    *out = g_stats;
    pthread_mutex_unlock(&g_lock);
}
