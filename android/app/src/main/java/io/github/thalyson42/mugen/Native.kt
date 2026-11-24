package io.github.thalyson42.mugen

/** JNI entry points of libmugen.so (phase P1: the empty engine library). */
object Native {
    init {
        System.loadLibrary("mugen")
    }

    /** Versions of the statically linked libraries, one per line. */
    @JvmStatic
    external fun libraryVersions(): String
}
