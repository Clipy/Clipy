// Version information
#define REALM_VERSION ""

// Specific headers
#define HAVE_MALLOC_H 0

// Realm-specific configuration
#define REALM_MAX_BPNODE_SIZE 1000
/* #undef REALM_MAX_BPNODE_SIZE_DEBUG */
#define REALM_ENABLE_ASSERTIONS 0
#define REALM_ENABLE_ALLOC_SET_ZERO 0
#define REALM_ENABLE_ENCRYPTION 1
#define REALM_ENABLE_MEMDEBUG 0
#define REALM_VALGRIND 0
#define REALM_METRICS 1
#define REALM_ASAN 0
#define REALM_TSAN 0

#define REALM_INSTALL_PREFIX "/usr/local"
#define REALM_INSTALL_INCLUDEDIR "include"
#define REALM_INSTALL_BINDIR "bin"
#define REALM_INSTALL_LIBDIR "lib"
#define REALM_INSTALL_LIBEXECDIR "libexec"
#define REALM_INSTALL_EXEC_PREFIX "/usr/local"
