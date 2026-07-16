/*
 * Copyright (C) 2026 Lohann Paterno Coutinho Ferreira (developer@lohann.dev)
 * 
 * Mach2Hex0 is a helper programn for converting an mach-o executable to Hex0 file.
 * This tool is system agnostic, must work in any system with a minimal C Compiler and libc.
 * 
 * Mach-O Reference:
 * - https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/EXTERNAL_HEADERS/mach-o/loader.h#L50-L81
 * - https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/mach/machine.h#L127-L482
 * - https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/bsd/kern/mach_loader.c#L1225-L1996
 * - https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/MachOTopics/1-Articles/executing_files.html
 * - https://github.com/aidansteele/osx-abi-macho-file-format-reference
 * - https://www.macsyscalls.com/en/syscall
 *
 * Hex0 Format:
 * - https://bootstrapping.miraheze.org/wiki/Hex0
 * 
 * Oficial Git Repo: https://github.com/Lohann/stage0-macos
 */

#include <stdint.h>

/******************************/
/* Detect C Compiler Features */
/******************************/

/* Detect target endianess */
#if defined(_MSC_VER) \
    || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ \
    || (defined(__LITTLE_ENDIAN__) && __LITTLE_ENDIAN__)
#   define CC_LITTLE_ENDIAN 1
#   define CC_BIG_ENDIAN 0
#else
#   define CC_LITTLE_ENDIAN 0
#   define CC_BIG_ENDIAN 1
#endif

/* Convenience macro to test the version of gcc.
 * Note: only works for GCC 2.0 and later */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#   define CC_GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#   define CC_GNUC_PREREQ(maj, min) 0
#endif

/* Detect C Compiler '__has_builtin' support */
#if defined(__has_builtin)
#   define CC_has_builtin(builtin) __has_builtin(__builtin_##builtin)
#else
#   define CC_has_builtin(builtin) 0
#endif

/* Detect C Compiler '__has_attribute' builtin support */
#if defined(__has_attribute)
#   define CC_has_attribute(attribute) __has_attribute(attribute)
#else
#   define CC_has_attribute(attribute) 0
#endif

/* Detect C Compiler '__has_include' builtin support */
#if defined(__has_include)
#   define CC_include(include) __has_include(include)
#else
#   define CC_include(include) 0
#endif

/* Detect C Compiler 'aligned(alignment)' attribute support */
#if CC_has_attribute(aligned) || defined(__TINYC__)
#   define CC_under_align(alignment) __attribute__((aligned(alignment)))
#elif defined(_MSC_VER)
#   define CC_under_align(alignment) __declspec(align(alignment))
#else
#   define CC_under_align
#endif

/* Detect C Compiler 'alignment' attribute support */
#if __STDC_VERSION__ >= 202311L
#   define CC_align(alignment) alignas(alignment)
#elif __STDC_VERSION__ >= 201112L
#   define CC_align(alignment) _Alignas(alignment)
#else
#   define CC_align(alignment) CC_under_align(alignment)
#endif

/* Detect C Compiler 'variadic arguments' support */
#if CC_include(<stdbool.h>)
#   include <stdarg.h>
#elif CC_has_builtin(va_arg)
    typedef __builtin_va_list va_list;
#   define va_end(ap) __builtin_va_end(ap)
#   define va_arg(ap, type) __builtin_va_arg(ap, type)
#   if __STDC_VERSION__ >= 202311L
        /* C23 uses a special builtin for var_start. */
#       define va_start(...) __builtin_c23_va_start(__VA_ARGS__)
#   else
        /* Versions before C23 do require the second parameter. */
#       define va_start(ap, param) __builtin_va_start(ap, param)
#   endif
#else
#   error "C Compiler doesn't support variadic arguments"
#endif

/* Portably define 'bool' type. */
#if __STDC_VERSION__ >= 202311L
    /* bool, true, and false are provided by the language in C23. */
#elif __STDC_VERSION__ >= 199901L && CC_include(<stdbool.h>)
#   include <stdbool.h>
#else
    typedef char bool;
#   define false 0
#   define true  1
#endif

/* Detect C Compiler 'restrict' attribute support */
#undef CC_restrict
#undef restrict
#undef __restrict
#if __STDC_VERSION__ >= 199901L
#   define CC_restrict restrict
#elif defined(__GNUC__) || defined(__TINYC__)
#   define CC_restrict __restrict
#else
#   define CC_restrict
#endif

/* Detect C Compiler 'noreturn' attribute support */
#if __STDC_VERSION__ >= 202311L
#   define CC_noreturn [[noreturn]]
#elif __STDC_VERSION__ >= 201112L
#   define CC_noreturn _Noreturn
#elif CC_has_attribute(noreturn) || defined(__GNUC__) || defined(__TINYC__)
#   define CC_noreturn __attribute__((noreturn))
#elif defined(_MSC_VER)
#   define CC_noreturn __declspec(noreturn)
#else
#   define CC_noreturn
#endif

/* Detect C Compiler '__may_alias__' attribute support */
#if CC_has_attribute(__may_alias__) || CC_GNUC_PREREQ(7, 1) || defined(__clang__)
#   define CC_may_alias __attribute__((__may_alias__))
#else
#   define CC_may_alias
#endif

/* Detect C Compiler 'nonnull' attribute support */
#if defined(__clang__) || CC_GNUC_PREREQ(3, 3) /* Attribute `nonnull' was valid as of gcc 3.3. */
#   define CC_nonnull __attribute__((nonnull))
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#   define CC_nonnull __declspec(nonnull)
#else
#   define CC_nonnull
#endif

/* Detect C Compiler 'inline' attribute support
 * inline requires special treatment; it's in C99, 
 * and GCC >=2.7 supports it too, but it's not in C89. */
#undef inline
#if (!defined(__cplusplus) && __STDC_VERSION__ >= 199901L) \
    || defined(__cplusplus) || defined(__clang__) \
    || (defined(__SUNPRO_C) && defined(__C99FEATURES__))
    /* it's a keyword */
#else
#   if CC_GNUC_PREREQ(2, 7)
#       define inline __inline__   /* __inline__ prevents -pedantic warnings */
#   else
#       define inline /* nothing */
#   endif
#endif

/* Detect C Compiler '__builtin_bswap16' support */
#if CC_has_builtin(bswap16) || defined(__GNUC__)
# define CC_bswap16(val) __builtin_bswap16((uint16_t)val)
#else
    static inline uint16_t CC_bswap16(uint16_t val) {
        return (uint16_t)( (val & UINT16_C(0xFF00)) >> 8 ) |
               (uint16_t)( (val & UINT16_C(0x00FF)) << 8 );
    }
#endif

/* Detect C Compiler '__builtin_bswap32' support */
#if CC_has_builtin(bswap32) || defined(__GNUC__)
#   define CC_bswap32(val) __builtin_bswap32((uint32_t)val)
#else
    static inline uint32_t CC_bswap32(uint32_t val) {
        return
        (uint32_t)CC_bswap16((uint16_t)( (val & UINT32_C(0x0000FFFF)) >>  0 )) << 16 |
        (uint32_t)CC_bswap16((uint16_t)( (val & UINT32_C(0xFFFF0000)) >> 16 )) >>  0;
    }
#endif

/* Detect C Compiler '__builtin_bswap64' support */
#if CC_has_builtin(bswap64) || defined(__GNUC__)
#   define CC_bswap64(val) __builtin_bswap64((uint64_t)val)
#else
    static inline uint64_t CC_bswap64(uint64_t val) {
        return 
        (uint64_t)CC_bswap32((uint32_t)( (val & UINT64_C(0x00000000FFFFFFFF)) >>  0 )) << 32 |
        (uint64_t)CC_bswap32((uint32_t)( (val & UINT64_C(0xFFFFFFFF00000000)) >> 32 )) >>  0;
    }
#endif

// Detect C compiler `native 128-bit integer` support
#if defined(__SIZEOF_INT128__)
    typedef signed   __int128 int128_t;
    typedef unsigned __int128 uint128_t;
    typedef signed   __int128 int_fast128_t;
    typedef unsigned __int128 uint_fast128_t;
    typedef signed   __int128 int_least128_t;
    typedef unsigned __int128 uint_least128_t;
#   define UINT128_MAX         ((uint128_t)-1)
#   define INT128_MAX          ((int128_t)+(UINT128_MAX/2))
#   define INT128_MIN          (-INT128_MAX-1)
#   define UINT_LEAST128_MAX   UINT128_MAX
#   define INT_LEAST128_MAX    INT128_MAX
#   define INT_LEAST128_MIN    INT128_MIN
#   define UINT_FAST128_MAX    UINT128_MAX
#   define INT_FAST128_MAX     INT128_MAX
#   define INT_FAST128_MIN     INT128_MIN
#   define INT128_WIDTH        128
#   define UINT128_WIDTH       128
#   define INT_LEAST128_WIDTH  128
#   define UINT_LEAST128_WIDTH 128
#   define INT_FAST128_WIDTH   128
#   define UINT_FAST128_WIDTH  128
#   if UINT128_WIDTH > __LLONG_WIDTH__
#       define INT128_C(N)         ((int_least128_t)+N ## WB)
#       define UINT128_C(N)        ((uint_least128_t)+N ## WBU)
#   else
#       define INT128_C(N)         ((int_least128_t)+N ## LL)
#       define UINT128_C(N)        ((uint_least128_t)+N ## LLU)
#   endif
#   define CC_make_u128(hi, lo) (((uint128_t)hi)<<64|((uint128_t)lo))
#   define CC_make_i128(hi, lo) ((int128_t)CC_make_u128(hi, lo))
#else
    // Fallback emulation struct.
#   if CC_LITTLE_ENDIAN
        typedef struct { CC_align(16) uint64_t lo; uint64_t hi; } uint128_t;
        typedef struct { CC_align(16) uint64_t lo;  int64_t hi; } int128_t;
#   else
        typedef struct { CC_align(16) uint64_t hi; uint64_t lo; } uint128_t;
        typedef struct { CC_align(16)  int64_t hi; uint64_t lo; } int128_t;
#   endif
#   define CC_init_u128(hi, lo) ((uint128_t){ .h##i = ((uint64_t)hi), .l##o = ((uint64_t)lo) })
#   define CC_init_i128(hi, lo) ((uint128_t){ .h##i = ((int64_t)hi),  .l##o = ((uint64_t)lo) })
#endif

/************************/
/* structs and typedefs */
/************************/

typedef int32_t cpu_type_t;
typedef int32_t cpu_subtype_t;

/*
 * The 32-bit mach header appears at the very beginning of the object file for
 * 32-bit architectures.
 */
struct mach_header {
    uint32_t magic;           /* mach magic number identifier */
    cpu_type_t cputype;       /* cpu specifier */
    cpu_subtype_t cpusubtype; /* machine specifier */
    uint32_t filetype;        /* type of file */
    uint32_t ncmds;           /* number of load commands */
    uint32_t sizeofcmds;      /* the size of all the load commands */
    uint32_t flags;           /* flags */
};
/* Constant for the magic field of the mach_header (32-bit architectures) */
#define	MH_MAGIC 0xfeedface	/* the mach magic number */
#define MH_CIGAM 0xcefaedfe	/* NXSwapInt(MH_MAGIC) */

/*
 * The 64-bit mach header appears at the very beginning of object files for
 * 64-bit architectures.
 */
struct mach_header_64 {
	uint32_t      magic;		/* mach magic number identifier */
	cpu_type_t	  cputype;	/* cpu specifier */
	cpu_subtype_t cpusubtype;	/* machine specifier */
	uint32_t	  filetype;	/* type of file */
	uint32_t	  ncmds;		/* number of load commands */
	uint32_t	  sizeofcmds;	/* the size of all the load commands */
	uint32_t	  flags;		/* flags */
	uint32_t	  reserved;	/* reserved */
};
/* Constant for the magic field of the mach_header_64 (64-bit architectures) */
#define MH_MAGIC_64 0xfeedfacf /* the 64-bit mach magic number */
#define MH_CIGAM_64 0xcffaedfe /* NXSwapInt(MH_MAGIC_64) */

struct fat_header {
    uint32_t magic;
    uint32_t nfat_arch;
};

struct fat_arch {
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    uint32_t offset;
    uint32_t size;
};

/// The uuid load command contains a single 128-bit unique random number that
/// identifies an object produced by the static link editor.
struct uuid_command {
    /// LC_UUID
    uint32_t cmd;

    /// sizeof(struct uuid_command)
    uint32_t cmdsize;

    /// the 128-bit uuid
    uint8_t uuid[16];
};

/// The version_min_command contains the min OS version on which this
/// binary was built to run.
struct version_min_command {
    /// LC_VERSION_MIN_MACOSX or LC_VERSION_MIN_IPHONEOS or LC_VERSION_MIN_WATCHOS or LC_VERSION_MIN_TVOS
    uint32_t cmd;

    /// sizeof(struct version_min_command)
    uint32_t cmdsize;

    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    uint32_t version;

    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    uint32_t sdk;
};

/// The source_version_command is an optional load command containing
/// the version of the sources used to build the binary.
struct source_version_command {
    /// LC_SOURCE_VERSION
    uint32_t cmd;

    /// sizeof(source_version_command)
    uint32_t cmdsize;

    /// A.B.C.D.E packed as a24.b10.c10.d10.e10
    uint64_t version;
};

/// The build_version_command contains the min OS version on which this
/// binary was built to run for its platform. The list of known platforms and
/// tool values following it.
struct build_version_command {
    /// LC_BUILD_VERSION
    uint32_t cmd;

    /// sizeof(struct build_version_command) plus
    /// ntools * sizeof(struct build_version_command)
    uint32_t cmdsize;

    /// platform
    uint32_t platform;

    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    uint32_t minos;

    /// X.Y.Z is encoded in nibbles xxxx.yy.zz
    uint32_t sdk;

    /// number of tool entries following this
    uint32_t ntools;
};

#define TOOL_CLANG 0x1
#define TOOL_SWIFT 0x2
#define TOOL_LD 0x3
// LLVM's stock LLD linker
#define TOOL_LLD 0x4
// Unofficially Zig
#define TOOL_ZIG 0x5

struct build_tool_version {
    /// enum for the tool
    uint32_t tool;

    /// version number of the tool
    uint32_t version;
};

#define PLATFORM_UNKNOWN 0
#define PLATFORM_ANY 0xffffffff
#define PLATFORM_MACOS 1
#define PLATFORM_IOS 2
#define PLATFORM_TVOS 3
#define PLATFORM_WATCHOS 4
#define PLATFORM_BRIDGEOS 5
#define PLATFORM_MACCATALYST 6
#define PLATFORM_IOSSIMULATOR 7
#define PLATFORM_TVOSSIMULATOR 8
#define PLATFORM_WATCHOSSIMULATOR 9
#define PLATFORM_DRIVERKIT 10
#define PLATFORM_VISIONOS 11
#define PLATFORM_VISIONOSSIMULATOR 12

/// The entry_point_command is a replacement for thread_command.
/// It is used for main executables to specify the location (file offset)
/// of main(). If -stack_size was used at link time, the stacksize
/// field will contain the stack size needed for the main thread.
struct entry_point_command
{
    /// LC_MAIN only used in MH_EXECUTE filetypes
    uint32_t cmd;

    /// sizeof(struct entry_point_command)
    uint32_t cmdsize;

    /// file (__TEXT) offset of main()
    uint64_t entryoff;

    /// if not zero, initial stack size
    uint64_t stacksize;
};

/// The symtab_command contains the offsets and sizes of the link-edit 4.3BSD
/// "stab" style symbol table information as described in the header files
/// <nlist.h> and <stab.h>.
struct symtab_command {
    /// LC_SYMTAB
    uint32_t cmd;

    /// sizeof(struct symtab_command)
    uint32_t cmdsize;

    /// symbol table offset
    uint32_t symoff;

    /// number of symbol table entries
    uint32_t nsyms;

    /// string table offset
    uint32_t stroff;

    /// string table size in bytes
    uint32_t strsize;
};

/// This is the second set of the symbolic information which is used to support
/// the data structures for the dynamically link editor.
///
/// The original set of symbolic information in the symtab_command which contains
/// the symbol and string tables must also be present when this load command is
/// present.  When this load command is present the symbol table is organized
/// into three groups of symbols:
///  local symbols (static and debugging symbols) - grouped by module
///  defined external symbols - grouped by module (sorted by name if not lib)
///  undefined external symbols (sorted by name if MH_BINDATLOAD is not set,
///  and in order the were seen by the static linker if MH_BINDATLOAD is set)
/// In this load command there are offsets and counts to each of the three groups
/// of symbols.
///
/// This load command contains a the offsets and sizes of the following new
/// symbolic information tables:
///  table of contents
///  module table
///  reference symbol table
///  indirect symbol table
/// The first three tables above (the table of contents, module table and
/// reference symbol table) are only present if the file is a dynamically linked
/// shared library.  For executable and object modules, which are files
/// containing only one module, the information that would be in these three
/// tables is determined as follows:
///  table of contents - the defined external symbols are sorted by name
///  module table - the file contains only one module so everything in the file
///  is part of the module.
///  reference symbol table - is the defined and undefined external symbols
///
/// For dynamically linked shared library files this load command also contains
/// offsets and sizes to the pool of relocation entries for all sections
/// separated into two groups:
///  external relocation entries
///  local relocation entries
/// For executable and object modules the relocation entries continue to hang
/// off the section structures.
struct dysymtab_command {
    /// LC_DYSYMTAB
    uint32_t cmd;

    /// sizeof(struct dysymtab_command)
    uint32_t cmdsize;

    // The symbols indicated by symoff and nsyms of the LC_SYMTAB load command
    // are grouped into the following three groups:
    //    local symbols (further grouped by the module they are from)
    //    defined external symbols (further grouped by the module they are from)
    //    undefined symbols
    //
    // The local symbols are used only for debugging.  The dynamic binding
    // process may have to use them to indicate to the debugger the local
    // symbols for a module that is being bound.
    //
    // The last two groups are used by the dynamic binding process to do the
    // binding (indirectly through the module table and the reference symbol
    // table when this is a dynamically linked shared library file).

    /// index of local symbols
    uint32_t ilocalsym;

    /// number of local symbols
    uint32_t nlocalsym;

    /// index to externally defined symbols
    uint32_t iextdefsym;

    /// number of externally defined symbols
    uint32_t nextdefsym;

    /// index to undefined symbols
    uint32_t iundefsym;

    /// number of undefined symbols
    uint32_t nundefsym;

    // For the for the dynamic binding process to find which module a symbol
    // is defined in the table of contents is used (analogous to the ranlib
    // structure in an archive) which maps defined external symbols to modules
    // they are defined in.  This exists only in a dynamically linked shared
    // library file.  For executable and object modules the defined external
    // symbols are sorted by name and is use as the table of contents.

    /// file offset to table of contents
    uint32_t tocoff;

    /// number of entries in table of contents
    uint32_t ntoc;

    // To support dynamic binding of "modules" (whole object files) the symbol
    // table must reflect the modules that the file was created from.  This is
    // done by having a module table that has indexes and counts into the merged
    // tables for each module.  The module structure that these two entries
    // refer to is described below.  This exists only in a dynamically linked
    // shared library file.  For executable and object modules the file only
    // contains one module so everything in the file belongs to the module.

    /// file offset to module table
    uint32_t modtaboff;

    /// number of module table entries
    uint32_t nmodtab;

    // To support dynamic module binding the module structure for each module
    // indicates the external references (defined and undefined) each module
    // makes.  For each module there is an offset and a count into the
    // reference symbol table for the symbols that the module references.
    // This exists only in a dynamically linked shared library file.  For
    // executable and object modules the defined external symbols and the
    // undefined external symbols indicates the external references.

    /// offset to referenced symbol table
    uint32_t extrefsymoff;

    /// number of referenced symbol table entries
    uint32_t nextrefsyms;

    // The sections that contain "symbol pointers" and "routine stubs" have
    // indexes and (implied counts based on the size of the section and fixed
    // size of the entry) into the "indirect symbol" table for each pointer
    // and stub.  For every section of these two types the index into the
    // indirect symbol table is stored in the section header in the field
    // reserved1.  An indirect symbol table entry is simply a 32bit index into
    // the symbol table to the symbol that the pointer or stub is referring to.
    // The indirect symbol table is ordered to match the entries in the section.

    /// file offset to the indirect symbol table
    uint32_t indirectsymoff;

    /// number of indirect symbol table entries
    uint32_t nindirectsyms;

    // To support relocating an individual module in a library file quickly the
    // external relocation entries for each module in the library need to be
    // accessed efficiently.  Since the relocation entries can't be accessed
    // through the section headers for a library file they are separated into
    // groups of local and external entries further grouped by module.  In this
    // case the presents of this load command who's extreloff, nextrel,
    // locreloff and nlocrel fields are non-zero indicates that the relocation
    // entries of non-merged sections are not referenced through the section
    // structures (and the reloff and nreloc fields in the section headers are
    // set to zero).
    //
    // Since the relocation entries are not accessed through the section headers
    // this requires the r_address field to be something other than a section
    // offset to identify the item to be relocated.  In this case r_address is
    // set to the offset from the vmaddr of the first LC_SEGMENT command.
    // For MH_SPLIT_SEGS images r_address is set to the the offset from the
    // vmaddr of the first read-write LC_SEGMENT command.
    //
    // The relocation entries are grouped by module and the module table
    // entries have indexes and counts into them for the group of external
    // relocation entries for that the module.
    //
    // For sections that are merged across modules there must not be any
    // remaining external relocation entries for them (for merged sections
    // remaining relocation entries must be local).

    /// offset to external relocation entries
    uint32_t extreloff;

    /// number of external relocation entries
    uint32_t nextrel;

    // All the local relocation entries are grouped together (they are not
    // grouped by their module since they are only used if the object is moved
    // from its statically link edited address).

    /// offset to local relocation entries
    uint32_t locreloff;

    /// number of local relocation entries
    uint32_t nlocrel;
};

/// The linkedit_data_command contains the offsets and sizes of a blob
/// of data in the __LINKEDIT segment.
struct linkedit_data_command {
    /// LC_CODE_SIGNATURE, LC_SEGMENT_SPLIT_INFO, LC_FUNCTION_STARTS, LC_DATA_IN_CODE, LC_DYLIB_CODE_SIGN_DRS or LC_LINKER_OPTIMIZATION_HINT.
    uint32_t cmd;

    /// sizeof(struct linkedit_data_command)
    uint32_t cmdsize;

    /// file offset of data in __LINKEDIT segment
    uint32_t dataoff;

    /// file size of data in __LINKEDIT segment
    uint32_t datasize;
};

/// The dyld_info_command contains the file offsets and sizes of
/// the new compressed form of the information dyld needs to
/// load the image.  This information is used by dyld on Mac OS X
/// 10.6 and later.  All information pointed to by this command
/// is encoded using byte streams, so no endian swapping is needed
/// to interpret it.
struct load_command {
    /// LC_DYLD_INFO or LC_DYLD_INFO_ONLY
    uint32_t cmd;

    /// sizeof(struct dyld_info_command)
    uint32_t cmdsize;
};

// Dyld rebases an image whenever dyld loads it at an address different
// from its preferred address.  The rebase information is a stream
// of byte sized opcodes whose symbolic names start with REBASE_OPCODE_.
// Conceptually the rebase information is a table of tuples:
//    <seg-index, seg-offset, type>
// The opcodes are a compressed way to encode the table by only
// encoding when a column changes.  In addition simple patterns
// like "every n'th offset for m times" can be encoded in a few
// bytes.
struct dyld_info_command {
    /// LC_DYLD_INFO or LC_DYLD_INFO_ONLY
    uint32_t cmd;

    /// sizeof(struct dyld_info_command)
    uint32_t cmdsize;

    /// file offset to rebase info
    uint32_t rebase_off;

    /// size of rebase info
    uint32_t rebase_size;

    // Dyld binds an image during the loading process, if the image
    // requires any pointers to be initialized to symbols in other images.
    // The bind information is a stream of byte sized
    // opcodes whose symbolic names start with BIND_OPCODE_.
    // Conceptually the bind information is a table of tuples:
    //    <seg-index, seg-offset, type, symbol-library-ordinal, symbol-name, addend>
    // The opcodes are a compressed way to encode the table by only
    // encoding when a column changes.  In addition simple patterns
    // like for runs of pointers initialized to the same value can be
    // encoded in a few bytes.

    /// file offset to binding info
    uint32_t bind_off;

    /// size of binding info
    uint32_t bind_size;

    // Some C++ programs require dyld to unique symbols so that all
    // images in the process use the same copy of some code/data.
    // This step is done after binding. The content of the weak_bind
    // info is an opcode stream like the bind_info.  But it is sorted
    // alphabetically by symbol name.  This enable dyld to walk
    // all images with weak binding information in order and look
    // for collisions.  If there are no collisions, dyld does
    // no updating.  That means that some fixups are also encoded
    // in the bind_info.  For instance, all calls to "operator new"
    // are first bound to libstdc++.dylib using the information
    // in bind_info.  Then if some image overrides operator new
    // that is detected when the weak_bind information is processed
    // and the call to operator new is then rebound.

    /// file offset to weak binding info
    uint32_t weak_bind_off;

    /// size of weak binding info
    uint32_t weak_bind_size;

    // Some uses of external symbols do not need to be bound immediately.
    // Instead they can be lazily bound on first use.  The lazy_bind
    // are contains a stream of BIND opcodes to bind all lazy symbols.
    // Normal use is that dyld ignores the lazy_bind section when
    // loading an image.  Instead the static linker arranged for the
    // lazy pointer to initially point to a helper function which
    // pushes the offset into the lazy_bind area for the symbol
    // needing to be bound, then jumps to dyld which simply adds
    // the offset to lazy_bind_off to get the information on what
    // to bind.

    /// file offset to lazy binding info
    uint32_t lazy_bind_off;

    /// size of lazy binding info
    uint32_t lazy_bind_size;

    // The symbols exported by a dylib are encoded in a trie.  This
    // is a compact representation that factors out common prefixes.
    // It also reduces LINKEDIT pages in RAM because it encodes all
    // information (name, address, flags) in one small, contiguous range.
    // The export area is a stream of nodes.  The first node sequentially
    // is the start node for the trie.
    //
    // Nodes for a symbol start with a uleb128 that is the length of
    // the exported symbol information for the string so far.
    // If there is no exported symbol, the node starts with a zero byte.
    // If there is exported info, it follows the length.
    //
    // First is a uleb128 containing flags. Normally, it is followed by
    // a uleb128 encoded offset which is location of the content named
    // by the symbol from the mach_header for the image.  If the flags
    // is EXPORT_SYMBOL_FLAGS_REEXPORT, then following the flags is
    // a uleb128 encoded library ordinal, then a zero terminated
    // UTF8 string.  If the string is zero length, then the symbol
    // is re-export from the specified dylib with the same name.
    // If the flags is EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER, then following
    // the flags is two uleb128s: the stub offset and the resolver offset.
    // The stub is used by non-lazy pointers.  The resolver is used
    // by lazy pointers and must be called to get the actual address to use.
    //
    // After the optional exported symbol information is a byte of
    // how many edges (0-255) that this node has leaving it,
    // followed by each edge.
    // Each edge is a zero terminated UTF8 of the addition chars
    // in the symbol, followed by a uleb128 offset for the node that
    // edge points to.

    /// file offset to lazy binding info
    uint32_t export_off;

    /// size of lazy binding info
    uint32_t export_size;
};

/// A program that uses a dynamic linker contains a dylinker_command to identify
/// the name of the dynamic linker (LC_LOAD_DYLINKER). And a dynamic linker
/// contains a dylinker_command to identify the dynamic linker (LC_ID_DYLINKER).
/// A file can have at most one of these.
/// This struct is also used for the LC_DYLD_ENVIRONMENT load command and contains
/// string for dyld to treat like an environment variable.
struct dylinker_command {
    /// LC_ID_DYLINKER, LC_LOAD_DYLINKER, or LC_DYLD_ENVIRONMENT
    uint32_t cmd;

    /// includes pathname string
    uint32_t cmdsize;

    /// A variable length string in a load command is represented by an lc_str
    /// union.  The strings are stored just after the load command structure and
    /// the offset is from the start of the load command structure.  The size
    /// of the string is reflected in the cmdsize field of the load command.
    /// Once again any padded bytes to bring the cmdsize field to a multiple
    /// of 4 bytes must be zero.
    uint32_t name;
};

/// Dynamically linked shared libraries are identified by two things.  The
/// pathname (the name of the library as found for execution), and the
/// compatibility version number.  The pathname must match and the compatibility
/// number in the user of the library must be greater than or equal to the
/// library being used.  The time stamp is used to record the time a library was
/// built and copied into user so it can be use to determined if the library used
/// at runtime is exactly the same as used to build the program.
struct dylib {
    /// library's pathname (offset pointing at the end of dylib_command)
    uint32_t name;

    /// library's build timestamp
    uint32_t timestamp;

    /// library's current version number
    uint32_t current_version;

    /// library's compatibility version number
    uint32_t compatibility_version;
};

/// A dynamically linked shared library (filetype= MH_DYLIB in the mach header)
/// contains a dylib_command (cmd= LC_ID_DYLIB) to identify the library.
/// An object that uses a dynamically linked shared library also contains a
/// dylib_command (cmd= LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, or
/// LC_REEXPORT_DYLIB) for each library it uses.
struct dylib_command {
    /// LC_ID_DYLIB, LC_LOAD_WEAK_DYLIB, LC_LOAD_DYLIB, LC_REEXPORT_DYLIB
    uint32_t cmd;

    /// includes pathname string
    uint32_t cmdsize;

    /// the library identification
    struct dylib dylib;
};

/// The rpath_command contains a path which at runtime should be added to the current
/// run path used to find @rpath prefixed dylibs.
struct rpath_command {
    /// LC_RPATH
    uint32_t cmd;

    /// includes string
    uint32_t cmdsize;

    /// path to add to run path
    uint32_t path;
};

typedef uint32_t vm_prot_t;

/// The segment load command indicates that a part of this file is to be
/// mapped into the task's address space.  The size of this segment in memory,
/// vmsize, maybe equal to or larger than the amount to map from this file,
/// filesize.  The file is mapped starting at fileoff to the beginning of
/// the segment in memory, vmaddr.  The rest of the memory of the segment,
/// if any, is allocated zero fill on demand.  The segment's maximum virtual
/// memory protection and initial virtual memory protection are specified
/// by the maxprot and initprot fields.  If the segment has sections then the
/// section structures directly follow the segment command and their size is
/// reflected in cmdsize.
struct segment_command {/* for 32-bit architectures */
    uint32_t cmd;       /* LC_SEGMENT */
    uint32_t cmdsize;   /* includes sizeof section structs */
    char segname[16];   /* segment name */
    uint32_t vmaddr;    /* memory address of this segment */
    uint32_t vmsize;    /* memory size of this segment */
    uint32_t fileoff;   /* file offset of this segment */
    uint32_t filesize;  /* amount to map from the file */
    vm_prot_t maxprot;  /* maximum VM protection */
    vm_prot_t initprot; /* initial VM protection */
    uint32_t nsects;    /* number of sections in segment */
    uint32_t flags;     /* flags */
};

/// A segment is made up of zero or more sections.  Non-MH_OBJECT files have
/// all of their segments with the proper sections in each, and padded to the
/// specified segment alignment when produced by the link editor.  The first
/// segment of a MH_EXECUTE and MH_FVMLIB format file contains the mach_header
/// and load commands of the object file before its first section.  The zero
/// fill sections are always last in their segment (in all formats).  This
/// allows the zeroed segment padding to be mapped into memory where zero fill
/// sections might be. The gigabyte zero fill sections, those with the section
/// type S_GB_ZEROFILL, can only be in a segment with sections of this type.
/// These segments are then placed after all other segments.
///
/// The MH_OBJECT format has all of its sections in one segment for
/// compactness.  There is no padding to a specified segment boundary and the
/// mach_header and load commands are not part of the segment.
///
/// Sections with the same section name, sectname, going into the same segment,
/// segname, are combined by the link editor.  The resulting section is aligned
/// to the maximum alignment of the combined sections and is the new section's
/// alignment.  The combined sections are aligned to their original alignment in
/// the combined section.  Any padded bytes to get the specified alignment are
/// zeroed.
///
/// The format of the relocation entries referenced by the reloff and nreloc
/// fields of the section structure for mach object files is described in the
/// header file <reloc.h>.
struct section {
    /// name of this section
    uint8_t sectname[16];

    /// segment this section goes in
    uint8_t segname[16];

    /// memory address of this section
    uint32_t addr;

    /// size in bytes of this section
    uint32_t size;

    /// file offset of this section
    uint32_t offset;

    /// section alignment (power of 2)
    uint32_t _gap;

    /// file offset of relocation entries
    uint32_t reloff;

    /// number of relocation entries
    uint32_t nreloc;

    /// flags (section type and attributes
    uint32_t flags;

    /// reserved (for offset or index)
    uint32_t reserved1;

    /// reserved (for count or sizeof)
    uint32_t reserved2;
};

/*
 * The 64-bit segment load command indicates that a part of this file is to be
 * mapped into a 64-bit task's address space.  If the 64-bit segment has
 * sections then section_64 structures directly follow the 64-bit segment
 * command and their size is reflected in cmdsize.
 */
struct segment_command_64 {/* for 64-bit architectures */
	uint32_t  cmd;         /* LC_SEGMENT_64 */
	uint32_t  cmdsize;	   /* includes sizeof section_64 structs */
	char      segname[16]; /* segment name */
	uint64_t  vmaddr;      /* memory address of this segment */
	uint64_t  vmsize;      /* memory size of this segment */
	uint64_t  fileoff;     /* file offset of this segment */
	uint64_t  filesize;    /* amount to map from the file */
	vm_prot_t maxprot;     /* maximum VM protection */
	vm_prot_t initprot;    /* initial VM protection */
	uint32_t  nsects;      /* number of sections in segment */
	uint32_t  flags;       /* flags */
};
struct section_64 {        /* for 64-bit architectures */
	char     sectname[16]; /* name of this section */
	char     segname[16];  /* segment this section goes in */
	uint64_t addr;         /* memory address of this section */
	uint64_t size;         /* size in bytes of this section */
	uint32_t offset;       /* file offset of this section */
	uint32_t align;        /* section alignment (power of 2) */
	uint32_t reloff;       /* file offset of relocation entries */
	uint32_t nreloc;       /* number of relocation entries */
	uint32_t flags;        /* flags (section type and attributes)*/
	uint32_t reserved1;    /* reserved (for offset or index) */
	uint32_t reserved2;    /* reserved (for count or sizeof) */
	uint32_t reserved3;    /* reserved */
};

struct nlist {
    uint32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    int16_t n_desc;
    uint32_t n_value;
};

/*
 * Thread commands contain machine-specific data structures suitable for
 * use in the thread state primitives.  The machine specific data structures
 * follow the struct thread_command as follows.
 * Each flavor of machine specific data structure is preceded by an uint32_t
 * constant for the flavor of that data structure, an uint32_t that is the
 * count of uint32_t's of the size of the state data structure and then
 * the state data structure follows.  This triple may be repeated for many
 * flavors.  The constants for the flavors, counts and state data structure
 * definitions are expected to be in the header file <machine/thread_status.h>.
 * These machine specific data structures sizes must be multiples of
 * 4 bytes.  The cmdsize reflects the total size of the thread_command
 * and all of the sizes of the constants for the flavors, counts and state
 * data structures.
 *
 * For executable objects that are unix processes there will be one
 * thread_command (cmd == LC_UNIXTHREAD) created for it by the link-editor.
 * This is the same as a LC_THREAD, except that a stack is automatically
 * created (based on the shell's limit for the stack size).  Command arguments
 * and environment variables are copied onto that stack.
 */
struct unixthread_command {
    uint32_t cmd;       /* LC_UNIXTHREAD */
    uint32_t cmdsize;   /* total size of this command */
    uint32_t flavor;    /* flavor of thread state */
    uint32_t count;     /* count of uint32_t's in thread state */
    /* struct XXX_thread_state state   thread state for this flavor */
};

/// After MacOS X 10.1 when a new load command is added that is required to be
/// understood by the dynamic linker for the image to execute properly the
/// LC_REQ_DYLD bit will be or'ed into the load command constant.  If the dynamic
/// linker sees such a load command it it does not understand will issue a
/// "unknown load command required for execution" error and refuse to use the
/// image.  Other load commands without this bit that are not understood will
/// simply be ignored.
#define LC_REQ_DYLD 0x80000000
/// No load command - invalid
#define LC_NONE 0x0
/// segment of this file to be mapped
#define LC_SEGMENT 0x1
/// link-edit stab symbol table info
#define LC_SYMTAB 0x2
/// link-edit gdb symbol table info (obsolete)
#define LC_SYMSEG 0x3
/// thread
#define LC_THREAD 0x4
/// unix thread (includes a stack)
#define LC_UNIXTHREAD 0x5
/// load a specified fixed VM shared library
#define LC_LOADFVMLIB 0x6
/// fixed VM shared library identification
#define LC_IDFVMLIB 0x7
/// object identification info (obsolete)
#define LC_IDENT 0x8
/// fixed VM file inclusion (internal use)
#define LC_FVMFILE 0x9
/// prepage command (internal use)
#define LC_PREPAGE 0xa
/// dynamic link-edit symbol table info
#define LC_DYSYMTAB 0xb
/// load a dynamically linked shared library
#define LC_LOAD_DYLIB 0xc
/// dynamically linked shared lib ident
#define LC_ID_DYLIB 0xd
/// load a dynamic linker
#define LC_LOAD_DYLINKER 0xe
/// dynamic linker identification
#define LC_ID_DYLINKER 0xf
/// modules prebound for a dynamically
#define LC_PREBOUND_DYLIB 0x10
/// image routines
#define LC_ROUTINES 0x11
/// sub framework
#define LC_SUB_FRAMEWORK 0x12
/// sub umbrella
#define LC_SUB_UMBRELLA 0x13
/// sub client
#define LC_SUB_CLIENT 0x14
/// sub library
#define LC_SUB_LIBRARY 0x15
/// two-level namespace lookup hints
#define LC_TWOLEVEL_HINTS 0x16
/// prebind checksum
#define LC_PREBIND_CKSUM 0x17


/// load a dynamically linked shared library that is allowed to be missing
/// (all symbols are weak imported).
#define LC_LOAD_WEAK_DYLIB 0x18 | LC_REQ_DYLD
/// 64-bit segment of this file to be mapped
#define LC_SEGMENT_64 0x19
/// 64-bit image routines
#define LC_ROUTINES_64 0x1a
/// the uuid
#define LC_UUID 0x1b
/// runpath additions
#define LC_RPATH 0x1c | LC_REQ_DYLD
/// local of code signature
#define LC_CODE_SIGNATURE 0x1d
/// local of info to split segments
#define LC_SEGMENT_SPLIT_INFO 0x1e
/// load and re-export dylib
#define LC_REEXPORT_DYLIB 0x1f | LC_REQ_DYLD
/// delay load of dylib until first use
#define LC_LAZY_LOAD_DYLIB 0x20
/// encrypted segment information
#define LC_ENCRYPTION_INFO 0x21
/// compressed dyld information
#define LC_DYLD_INFO 0x22
/// compressed dyld information only
#define LC_DYLD_INFO_ONLY 0x22 | LC_REQ_DYLD
/// load upward dylib
#define LC_LOAD_UPWARD_DYLIB 0x23 | LC_REQ_DYLD
/// build for MacOSX min OS version
#define LC_VERSION_MIN_MACOSX 0x24
/// build for iPhoneOS min OS version
#define LC_VERSION_MIN_IPHONEOS 0x25
/// compressed table of function start addresses
#define LC_FUNCTION_STARTS 0x26
/// string for dyld to treat like environment variable
#define LC_DYLD_ENVIRONMENT 0x27
/// replacement for LC_UNIXTHREAD
#define LC_MAIN 0x28 | LC_REQ_DYLD
/// table of non-instructions in __text
#define LC_DATA_IN_CODE 0x29
/// source version used to build binary
#define LC_SOURCE_VERSION 0x2A
/// Code signing DRs copied from linked dylibs
#define LC_DYLIB_CODE_SIGN_DRS 0x2B
/// 64-bit encrypted segment information
#define LC_ENCRYPTION_INFO_64 0x2C
/// linker options in MH_OBJECT files
#define LC_LINKER_OPTION 0x2D
/// optimization hints in MH_OBJECT files
#define LC_LINKER_OPTIMIZATION_HINT 0x2E
/// build for AppleTV min OS version
#define LC_VERSION_MIN_TVOS 0x2F
/// build for Watch min OS version
#define LC_VERSION_MIN_WATCHOS 0x30
/// arbitrary data included within a Mach-O file
#define LC_NOTE 0x31
/// build for platform min OS version
#define LC_BUILD_VERSION 0x32
/// used with linkedit_data_command, payload is trie
#define LC_DYLD_EXPORTS_TRIE 0x33 | LC_REQ_DYLD
/// used with linkedit_data_command
#define LC_DYLD_CHAINED_FIXUPS 0x34 | LC_REQ_DYLD
/// used with fileset_entry_command
#define LC_FILESET_ENTRY      (0x35 | LC_REQ_DYLD)

/*
 * The layout of the file depends on the filetype.  For all but the MH_OBJECT
 * file type the segments are padded out and aligned on a segment alignment
 * boundary for efficient demand pageing.  The MH_EXECUTE, MH_FVMLIB, MH_DYLIB,
 * MH_DYLINKER and MH_BUNDLE file types also have the headers included as part
 * of their first segment.
 * 
 * The file type MH_OBJECT is a compact format intended as output of the
 * assembler and input (and possibly output) of the link editor (the .o
 * format).  All sections are in one unnamed segment with no segment padding. 
 * This format is used as an executable format when the file is so small the
 * segment padding greatly increases its size.
 *
 * The file type MH_PRELOAD is an executable format intended for things that
 * are not executed under the kernel (proms, stand alones, kernels, etc).  The
 * format can be executed under the kernel but may demand paged it and not
 * preload it before execution.
 *
 * A core file is in MH_CORE format and can be any in an arbritray legal
 * Mach-O file.
 *
 * Constants for the filetype field of the mach_header
 */
/// relocatable object file
#define MH_OBJECT 0x1

/// demand paged executable file
#define MH_EXECUTE 0x2

/// fixed VM shared library file
#define MH_FVMLIB 0x3

/// core file
#define MH_CORE 0x4

/// preloaded executable file
#define MH_PRELOAD 0x5

/// dynamically bound shared library
#define MH_DYLIB 0x6

/// dynamic link editor
#define MH_DYLINKER 0x7

/// dynamically bound bundle file
#define MH_BUNDLE 0x8

/// shared library stub for static linking only, no section contents
#define MH_DYLIB_STUB 0x9

/// companion file with only debug sections
#define MH_DSYM 0xa

/// x86_64 kexts
#define MH_KEXT_BUNDLE 0xb

// Constants for the flags field of the mach_header

/// the object file has no undefined references
#define MH_NOUNDEFS 0x1

/// the object file is the output of an incremental link against a base file and can't be link edited again
#define MH_INCRLINK 0x2

/// the object file is input for the dynamic linker and can't be statically link edited again
#define MH_DYLDLINK 0x4

/// the object file's undefined references are bound by the dynamic linker when loaded.
#define MH_BINDATLOAD 0x8

/// the file has its dynamic undefined references prebound.
#define MH_PREBOUND 0x10

/// the file has its read-only and read-write segments split
#define MH_SPLIT_SEGS 0x20

/// the shared library init routine is to be run lazily via catching memory faults to its writeable segments (obsolete)
#define MH_LAZY_INIT 0x40

/// the image is using two-level name space bindings
#define MH_TWOLEVEL 0x80

/// the executable is forcing all images to use flat name space bindings
#define MH_FORCE_FLAT 0x100

/// this umbrella guarantees no multiple definitions of symbols in its sub-images so the two-level namespace hints can always be used.
#define MH_NOMULTIDEFS 0x200

/// do not have dyld notify the prebinding agent about this executable
#define MH_NOFIXPREBINDING 0x400

/// the binary is not prebound but can have its prebinding redone. only used when MH_PREBOUND is not set.
#define MH_PREBINDABLE 0x800

/// indicates that this binary binds to all two-level namespace modules of its dependent libraries. only used when MH_PREBINDABLE and MH_TWOLEVEL are both set.
#define MH_ALLMODSBOUND 0x1000

/// safe to divide up the sections into sub-sections via symbols for dead code stripping
#define MH_SUBSECTIONS_VIA_SYMBOLS 0x2000

/// the binary has been canonicalized via the unprebind operation
#define MH_CANONICAL 0x4000

/// the final linked image contains external weak symbols
#define MH_WEAK_DEFINES 0x8000

/// the final linked image uses weak symbols
#define MH_BINDS_TO_WEAK 0x10000

/// When this bit is set, all stacks in the task will be given stack execution privilege.  Only used in MH_EXECUTE filetypes.
#define MH_ALLOW_STACK_EXECUTION 0x20000

/// When this bit is set, the binary declares it is safe for use in processes with uid zero
#define MH_ROOT_SAFE 0x40000

/// When this bit is set, the binary declares it is safe for use in processes when issetugid() is true
#define MH_SETUID_SAFE 0x80000

/// When this bit is set on a dylib, the static linker does not need to examine dependent dylibs to see if any are re-exported
#define MH_NO_REEXPORTED_DYLIBS 0x100000

/// When this bit is set, the OS will load the main executable at a random address.  Only used in MH_EXECUTE filetypes.
#define MH_PIE 0x200000

/// Only for use on dylibs.  When linking against a dylib that has this bit set, the static linker will automatically not create a LC_LOAD_DYLIB load command to the dylib if no symbols are being referenced from the dylib.
#define MH_DEAD_STRIPPABLE_DYLIB 0x400000

/// Contains a section of type S_THREAD_LOCAL_VARIABLES
#define MH_HAS_TLV_DESCRIPTORS 0x800000

/// When this bit is set, the OS will run the main executable with a non-executable heap even on platforms (e.g. x86) that don't require it. Only used in MH_EXECUTE filetypes.
#define MH_NO_HEAP_EXECUTION 0x1000000

/// The code was linked for use in an application extension.
#define MH_APP_EXTENSION_SAFE 0x02000000

/// The external symbols listed in the nlist symbol table do not include all the symbols listed in the dyld info.
#define MH_NLIST_OUTOFSYNC_WITH_DYLDINFO 0x04000000

/// Allow LC_MIN_VERSION_MACOS and LC_BUILD_VERSION load commands with the platforms macOS, iOSMac, iOSSimulator, tvOSSimulator and watchOSSimulator.
#define MH_SIM_SUPPORT 0x08000000

/// Only for use on dylibs. When this bit is set, the dylib is part of the dyld shared cache, rather than loose in the filesystem.
#define MH_DYLIB_IN_CACHE 0x80000000

// Constants for the flags field of the fat_header

/// the fat magic number
#define FAT_MAGIC 0xcafebabe

/// NXSwapLong(FAT_MAGIC)
#define FAT_CIGAM 0xbebafeca

/// the 64-bit fat magic number
#define FAT_MAGIC_64 0xcafebabf

/// NXSwapLong(FAT_MAGIC_64)
#define FAT_CIGAM_64 0xbfbafeca

/// Segment flags
/// The file contents for this segment is for the high part of the VM space, the low part
/// is zero filled (for stacks in core files).
#define SG_HIGHVM 0x1
/// This segment is the VM that is allocated by a fixed VM library, for overlap checking in
/// the link editor.
#define SG_FVMLIB 0x2
/// This segment has nothing that was relocated in it and nothing relocated to it, that is
/// it maybe safely replaced without relocation.
#define SG_NORELOC 0x4
/// This segment is protected.  If the segment starts at file offset 0, the
/// first page of the segment is not protected.  All other pages of the segment are protected.
#define SG_PROTECTED_VERSION_1 0x8
/// This segment is made read-only after fixups
#define SG_READ_ONLY 0x10

/// The flags field of a section structure is separated into two parts a section
/// type and section attributes.  The section types are mutually exclusive (it
/// can only have one type) but the section attributes are not (it may have more
/// than one attribute).
/// 256 section types
#define SECTION_TYPE 0x000000ff

///  24 section attributes
#define SECTION_ATTRIBUTES 0xffffff00

/// regular section
#define S_REGULAR 0x0

/// zero fill on demand section
#define S_ZEROFILL 0x1

/// section with only literal C string
#define S_CSTRING_LITERALS 0x2

/// section with only 4 byte literals
#define S_4BYTE_LITERALS 0x3

/// section with only 8 byte literals
#define S_8BYTE_LITERALS 0x4

/// section with only pointers to
#define S_LITERAL_POINTERS 0x5

/// symbol is not in any section
#define	NO_SECT 0	

/// if any of these bits set, a symbolic debugging entry
#define N_STAB 0xe0

/// private external symbol bit
#define N_PEXT 0x10

/// mask for the type bits
#define N_TYPE 0x0e

/// external symbol bit, set for external symbols
#define N_EXT 0x01

/// symbol is undefined
#define N_UNDF 0x0

/// symbol is absolute
#define N_ABS 0x2

/// symbol is defined in the section number given in n_sect
#define N_SECT 0xe

/// symbol is undefined  and the image is using a prebound
/// value  for the symbol
#define N_PBUD 0xc

/// symbol is defined to be the same as another symbol the n_value
/// field is an index into the string table specifying the name of the
/// other symbol
#define N_INDR 0xa

/// global symbol: name,,NO_SECT,type,0
#define N_GSYM 0x20

/// procedure name (f77 kludge): name,,NO_SECT,0,0
#define N_FNAME 0x22

/// procedure: name,,n_sect,linenumber,address
#define N_FUN 0x24

/// static symbol: name,,n_sect,type,address
#define N_STSYM 0x26

/// .lcomm symbol: name,,n_sect,type,address
#define N_LCSYM 0x28

/// begin nsect sym: 0,,n_sect,0,address
#define N_BNSYM 0x2e

/// AST file path: name,,NO_SECT,0,0
#define N_AST 0x32

/// emitted with gcc2_compiled and in gcc source
#define N_OPT 0x3c

/// register sym: name,,NO_SECT,type,register
#define N_RSYM 0x40

/// src line: 0,,n_sect,linenumber,address
#define N_SLINE 0x44

/// end nsect sym: 0,,n_sect,0,address
#define N_ENSYM 0x4e

/// structure elt: name,,NO_SECT,type,struct_offset
#define N_SSYM 0x60

/// source file name: name,,n_sect,0,address
#define N_SO 0x64

/// object file name: name,,0,0,st_mtime
#define N_OSO 0x66

/// local sym: name,,NO_SECT,type,offset
#define N_LSYM 0x80

/// include file beginning: name,,NO_SECT,0,sum
#define N_BINCL 0x82

/// #included file name: name,,n_sect,0,address
#define N_SOL 0x84

/// compiler parameters: name,,NO_SECT,0,0
#define N_PARAMS 0x86

/// compiler version: name,,NO_SECT,0,0
#define N_VERSION 0x88

/// compiler -O level: name,,NO_SECT,0,0
#define N_OLEVEL 0x8A

/// parameter: name,,NO_SECT,type,offset
#define N_PSYM 0xa0

/// include file end: name,,NO_SECT,0,0
#define N_EINCL 0xa2

/// alternate entry: name,,n_sect,linenumber,address
#define N_ENTRY 0xa4

/// left bracket: 0,,NO_SECT,nesting level,address
#define N_LBRAC 0xc0

/// deleted include file: name,,NO_SECT,0,sum
#define N_EXCL 0xc2

/// right bracket: 0,,NO_SECT,nesting level,address
#define N_RBRAC 0xe0

/// begin common: name,,NO_SECT,0,0
#define N_BCOMM 0xe2

/// end common: name,,n_sect,0,0
#define N_ECOMM 0xe4

/// end common (local name): 0,,n_sect,0,address
#define N_ECOML 0xe8

/// second stab entry with length information
#define N_LENG 0xfe

// For the two types of symbol pointers sections and the symbol stubs section
// they have indirect symbol table entries.  For each of the entries in the
// section the indirect symbol table entries, in corresponding order in the
// indirect symbol table, start at the index stored in the reserved1 field
// of the section structure.  Since the indirect symbol table entries
// correspond to the entries in the section the number of indirect symbol table
// entries is inferred from the size of the section divided by the size of the
// entries in the section.  For symbol pointers sections the size of the entries
// in the section is 4 bytes and for symbol stubs sections the byte size of the
// stubs is stored in the reserved2 field of the section structure.

/// section with only non-lazy symbol pointers
#define S_NON_LAZY_SYMBOL_POINTERS 0x6

/// section with only lazy symbol pointers
#define S_LAZY_SYMBOL_POINTERS 0x7

/// section with only symbol stubs, byte size of stub in the reserved2 field
#define S_SYMBOL_STUBS 0x8

/// section with only function pointers for initialization
#define S_MOD_INIT_FUNC_POINTERS 0x9

/// section with only function pointers for termination
#define S_MOD_TERM_FUNC_POINTERS 0xa

/// section contains symbols that are to be coalesced
#define S_COALESCED 0xb

/// zero fill on demand section (that can be larger than 4 gigabytes)
#define S_GB_ZEROFILL 0xc

/// section with only pairs of function pointers for interposing
#define S_INTERPOSING 0xd

/// section with only 16 byte literals
#define S_16BYTE_LITERALS 0xe

/// section contains DTrace Object Format
#define S_DTRACE_DOF 0xf

/// section with only lazy symbol pointers to lazy loaded dylibs
#define S_LAZY_DYLIB_SYMBOL_POINTERS 0x10

// If a segment contains any sections marked with S_ATTR_DEBUG then all
// sections in that segment must have this attribute.  No section other than
// a section marked with this attribute may reference the contents of this
// section.  A section with this attribute may contain no symbols and must have
// a section type S_REGULAR.  The static linker will not copy section contents
// from sections with this attribute into its output file.  These sections
// generally contain DWARF debugging info.

/// a debug section
#define S_ATTR_DEBUG 0x02000000

/// section contains only true machine instructions
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000

/// section contains coalesced symbols that are not to be in a ranlib
/// table of contents
#define S_ATTR_NO_TOC 0x40000000

/// ok to strip static symbols in this section in files with the
/// MH_DYLDLINK flag
#define S_ATTR_STRIP_STATIC_SYMS 0x20000000

/// no dead stripping
#define S_ATTR_NO_DEAD_STRIP 0x10000000

/// blocks are live if they reference live blocks
#define S_ATTR_LIVE_SUPPORT 0x8000000

/// used with x86 code stubs written on by dyld
#define S_ATTR_SELF_MODIFYING_CODE 0x4000000

/// section contains some machine instructions
#define S_ATTR_SOME_INSTRUCTIONS 0x400

/// section has external relocation entries
#define S_ATTR_EXT_RELOC 0x200

/// section has local relocation entries
#define S_ATTR_LOC_RELOC 0x100

/// template of initial values for TLVs
#define S_THREAD_LOCAL_REGULAR 0x11

/// template of initial values for TLVs
#define S_THREAD_LOCAL_ZEROFILL 0x12

/// TLV descriptors
#define S_THREAD_LOCAL_VARIABLES 0x13

/// pointers to TLV descriptors
#define S_THREAD_LOCAL_VARIABLE_POINTERS 0x14

/// functions to call to initialize TLV values
#define S_THREAD_LOCAL_INIT_FUNCTION_POINTERS 0x15

/// 32-bit offsets to initializers
#define S_INIT_FUNC_OFFSETS 0x16

// The following are used to encode rebasing information
#define REBASE_TYPE_POINTER 1
#define REBASE_TYPE_TEXT_ABSOLUTE32 2
#define REBASE_TYPE_TEXT_PCREL32 3

#define REBASE_OPCODE_MASK 0xF0
#define REBASE_IMMEDIATE_MASK 0x0F
#define REBASE_OPCODE_DONE 0x00
#define REBASE_OPCODE_SET_TYPE_IMM 0x10
#define REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x20
#define REBASE_OPCODE_ADD_ADDR_ULEB 0x30
#define REBASE_OPCODE_ADD_ADDR_IMM_SCALED 0x40
#define REBASE_OPCODE_DO_REBASE_IMM_TIMES 0x50
#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES 0x60
#define REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB 0x70
#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB 0x80

// The following are used to encode binding information
#define BIND_TYPE_POINTER 1
#define BIND_TYPE_TEXT_ABSOLUTE32 2
#define BIND_TYPE_TEXT_PCREL32 3

#define BIND_SPECIAL_DYLIB_SELF 0
#define BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE -1
#define BIND_SPECIAL_DYLIB_FLAT_LOOKUP -2

#define BIND_SYMBOL_FLAGS_WEAK_IMPORT 0x1
#define BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION 0x8

#define BIND_OPCODE_MASK 0xf0
#define BIND_IMMEDIATE_MASK 0x0f
#define BIND_OPCODE_DONE 0x00
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 0x10
#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB 0x20
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM 0x30
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40
#define BIND_OPCODE_SET_TYPE_IMM 0x50
#define BIND_OPCODE_SET_ADDEND_SLEB 0x60
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x70
#define BIND_OPCODE_ADD_ADDR_ULEB 0x80
#define BIND_OPCODE_DO_BIND 0x90
#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB 0xa0
#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED 0xb0
#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB 0xc0
/*
 * Relocation types used in the arm64 implementation.
 * 
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/EXTERNAL_HEADERS/mach-o/arm64/reloc.h
 */
enum reloc_type_arm64
{
    ARM64_RELOC_UNSIGNED,            // for pointers
    ARM64_RELOC_SUBTRACTOR,          // must be followed by a ARM64_RELOC_UNSIGNED
    ARM64_RELOC_BRANCH26,            // a B/BL instruction with 26-bit displacement
    ARM64_RELOC_PAGE21,              // pc-rel distance to page of target
    ARM64_RELOC_PAGEOFF12,           // offset within page, scaled by r_length
    ARM64_RELOC_GOT_LOAD_PAGE21,     // pc-rel distance to page of GOT slot
    ARM64_RELOC_GOT_LOAD_PAGEOFF12,  // offset within page of GOT slot,
                                     //  scaled by r_length
    ARM64_RELOC_POINTER_TO_GOT,      // for pointers to GOT slots
    ARM64_RELOC_TLVP_LOAD_PAGE21,    // pc-rel distance to page of TLVP slot
    ARM64_RELOC_TLVP_LOAD_PAGEOFF12, // offset within page of TLVP slot,
                                     //  scaled by r_length
    ARM64_RELOC_ADDEND               // must be followed by PAGE21 or PAGEOFF12
};

/*
 * Relocation types used in the arm implementation.
 *
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/EXTERNAL_HEADERS/mach-o/arm/reloc.h
 */
enum reloc_type_arm
{
    ARM_RELOC_VANILLA,        /* generic relocation as discribed above */
    ARM_RELOC_PAIR,           /* the second relocation entry of a pair */
    ARM_RELOC_SECTDIFF,       /* a PAIR follows with subtract symbol value */
    ARM_RELOC_LOCAL_SECTDIFF, /* like ARM_RELOC_SECTDIFF, but the symbol referenced was local.  */
    ARM_RELOC_PB_LA_PTR,      /* prebound lazy pointer */
    ARM_RELOC_BR24,           /* 24 bit branch displacement (to a word address) */
    ARM_THUMB_RELOC_BR22,     /* 22 bit branch displacement (to a half-word address) */
    ARM_THUMB_32BIT_BRANCH,   /* obsolete - a thumb 32-bit branch instruction possibly needing page-spanning branch workaround */

    /*
     * For these two r_type relocations they always have a pair following them
     * and the r_length bits are used differently.  The encoding of the
     * r_length is as follows:
     * low bit of r_length:
     *  0 - :lower16: for movw instructions
     *  1 - :upper16: for movt instructions
     * high bit of r_length:
     *  0 - arm instructions
     *  1 - thumb instructions
     * the other half of the relocated expression is in the following pair
     * relocation entry in the the low 16 bits of r_address field.
     */
    ARM_RELOC_HALF,
    ARM_RELOC_HALF_SECTDIFF
};

/*
 * Relocation types used in the x86_64 implementation.
 *
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/EXTERNAL_HEADERS/mach-o/x86_64/reloc.h
 */
enum reloc_type_x86_64
{
    X86_64_RELOC_UNSIGNED,   // for absolute addresses
    X86_64_RELOC_SIGNED,     // for signed 32-bit displacement
    X86_64_RELOC_BRANCH,     // a CALL/JMP instruction with 32-bit displacement
    X86_64_RELOC_GOT_LOAD,   // a MOVQ load of a GOT entry
    X86_64_RELOC_GOT,        // other GOT references
    X86_64_RELOC_SUBTRACTOR, // must be followed by a X86_64_RELOC_UNSIGNED
    X86_64_RELOC_SIGNED_1,   // for signed 32-bit displacement with a -1 addend
    X86_64_RELOC_SIGNED_2,   // for signed 32-bit displacement with a -2 addend
    X86_64_RELOC_SIGNED_4,   // for signed 32-bit displacement with a -4 addend
    X86_64_RELOC_TLV,        // for thread local variables
};


/*
 * LC_DYLD_CHAINED_FIXUPS
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/EXTERNAL_HEADERS/mach-o/fixup-chains.h#L89 
 */
// header of the LC_DYLD_CHAINED_FIXUPS payload
struct dyld_chained_fixups_header
{
    uint32_t    fixups_version;    // 0
    uint32_t    starts_offset;     // offset of dyld_chained_starts_in_image in chain_data
    uint32_t    imports_offset;    // offset of imports table in chain_data
    uint32_t    symbols_offset;    // offset of symbol strings in chain_data
    uint32_t    imports_count;     // number of imported symbol names
    uint32_t    imports_format;    // DYLD_CHAINED_IMPORT*
    uint32_t    symbols_format;    // 0 => uncompressed, 1 => zlib compressed
};

// This struct is embedded in LC_DYLD_CHAINED_FIXUPS payload
struct dyld_chained_starts_in_image
{
    uint32_t    seg_count;
    uint32_t    seg_info_offset[1];  // each entry is offset into this struct for that segment
    // followed by pool of dyld_chain_starts_in_segment data
};

// This struct is embedded in dyld_chain_starts_in_image
// and passed down to the kernel for page-in linking
struct dyld_chained_starts_in_segment
{
    uint32_t    size;               // size of this (amount kernel needs to copy)
    uint16_t    page_size;          // 0x1000 or 0x4000
    uint16_t    pointer_format;     // DYLD_CHAINED_PTR_*
    uint64_t    segment_offset;     // offset in memory to start of segment
    uint32_t    max_valid_pointer;  // for 32-bit OS, any value beyond this is not a pointer
    uint16_t    page_count;         // how many pages are in array
    uint16_t    page_start[1];      // each entry is offset in each page of first element in chain
                                    // or DYLD_CHAINED_PTR_START_NONE if no fixups on page
 // uint16_t    chain_starts[1];    // some 32-bit formats may require multiple starts per page.
                                    // for those, if high bit is set in page_starts[], then it
                                    // is index into chain_starts[] which is a list of starts
                                    // the last of which has the high bit set
};

enum {
    DYLD_CHAINED_PTR_START_NONE   = 0xFFFF, // used in page_start[] to denote a page with no fixups
    DYLD_CHAINED_PTR_START_MULTI  = 0x8000, // used in page_start[] to denote a page which has multiple starts
    DYLD_CHAINED_PTR_START_LAST   = 0x8000, // used in chain_starts[] to denote last start in list for page
};

// This struct is embedded in __TEXT,__chain_starts section in firmware
struct dyld_chained_starts_offsets
{
    uint32_t    pointer_format;     // DYLD_CHAINED_PTR_32_FIRMWARE
    uint32_t    starts_count;       // number of starts in array
    uint32_t    chain_starts[1];    // array chain start offsets
};

// values for dyld_chained_starts_in_segment.pointer_format
enum {
    DYLD_CHAINED_PTR_ARM64E                 =  1,    // stride 8, unauth target is vmaddr
    DYLD_CHAINED_PTR_64                     =  2,    // target is vmaddr
    DYLD_CHAINED_PTR_32                     =  3,
    DYLD_CHAINED_PTR_32_CACHE               =  4,
    DYLD_CHAINED_PTR_32_FIRMWARE            =  5,
    DYLD_CHAINED_PTR_64_OFFSET              =  6,    // target is vm offset
    DYLD_CHAINED_PTR_ARM64E_OFFSET          =  7,    // old name
    DYLD_CHAINED_PTR_ARM64E_KERNEL          =  7,    // stride 4, unauth target is vm offset
    DYLD_CHAINED_PTR_64_KERNEL_CACHE        =  8,
    DYLD_CHAINED_PTR_ARM64E_USERLAND        =  9,    // stride 8, unauth target is vm offset
    DYLD_CHAINED_PTR_ARM64E_FIRMWARE        = 10,    // stride 4, unauth target is vmaddr
    DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE    = 11,    // stride 1, x86_64 kernel caches
    DYLD_CHAINED_PTR_ARM64E_USERLAND24      = 12,    // stride 8, unauth target is vm offset, 24-bit bind
    DYLD_CHAINED_PTR_ARM64E_SHARED_CACHE    = 13,    // stride 8, regular/auth targets both vm offsets.  Only A keys supported
};

// DYLD_CHAINED_PTR_ARM64E
typedef uint64_t dyld_chained_ptr_arm64e_rebase;
typedef uint64_t dyld_chained_ptr_arm64e_bind;
typedef uint64_t dyld_chained_ptr_arm64e_auth_rebase;
typedef uint64_t dyld_chained_ptr_arm64e_auth_bind;

// DYLD_CHAINED_PTR_64/DYLD_CHAINED_PTR_64_OFFSET
typedef uint64_t dyld_chained_ptr_64_rebase;

// DYLD_CHAINED_PTR_ARM64E_USERLAND24
typedef uint64_t dyld_chained_ptr_arm64e_bind24;
typedef uint64_t dyld_chained_ptr_arm64e_auth_bind24;

// DYLD_CHAINED_PTR_64
typedef uint64_t dyld_chained_ptr_64_bind;

// DYLD_CHAINED_PTR_64_KERNEL_CACHE, DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE
typedef uint64_t dyld_chained_ptr_64_kernel_cache_rebase;

// DYLD_CHAINED_PTR_32
// Note: for DYLD_CHAINED_PTR_32 some non-pointer values are co-opted into the chain
// as out of range rebases.  If an entry in the chain is > max_valid_pointer, then it
// is not a pointer.  To restore the value, subtract off the bias, which is
// (64MB+max_valid_pointer)/2.
typedef uint32_t dyld_chained_ptr_32_rebase;

// DYLD_CHAINED_PTR_32
typedef uint32_t dyld_chained_ptr_32_bind;

// DYLD_CHAINED_PTR_32_CACHE
typedef uint32_t dyld_chained_ptr_32_cache_rebase;

// DYLD_CHAINED_PTR_32_FIRMWARE
typedef uint32_t dyld_chained_ptr_32_firmware_rebase;

// DYLD_CHAINED_PTR_ARM64E_SHARED_CACHE
typedef uint64_t dyld_chained_ptr_arm64e_shared_cache_rebase;
typedef uint64_t dyld_chained_ptr_arm64e_shared_cache_auth_rebase;

// values for dyld_chained_fixups_header.imports_format
#define DYLD_CHAINED_IMPORT 1
#define DYLD_CHAINED_IMPORT_ADDEND 2
#define DYLD_CHAINED_IMPORT_ADDEND64 3

// DYLD_CHAINED_IMPORT
typedef uint32_t dyld_chained_import;

// DYLD_CHAINED_IMPORT_ADDEND
struct dyld_chained_import_addend
{
    uint32_t name_offset;
    int32_t     addend;
};

// DYLD_CHAINED_IMPORT_ADDEND64
struct dyld_chained_import_addend64
{
    uint16_t lib_ordinal;
    uint16_t weak_import;
    uint32_t name_offset;
    uint64_t addend;
};

/* 
 * SecCodeSignatureFlags
 * Reference:
 * https://github.com/apple-oss-distributions/Security/blob/db15acbe6a7f257a859ad9a3bb86097bfe0679d9/header_symlinks/Security/CSCommon.h#L251-L299
 */
// Indicates that the code may act as a host that controls and supervises guest
// code. If this flag is not set in a code signature, the code is never considered
// eligible to be a host, and any attempt to act like one will be ignored or rejected.
#define kSecCodeSignatureHost 0x0001 /* may host guest code */

// The code has been sealed without a signing identity. No identity may be retrieved
// from it, and any code requirement placing restrictions on the signing identity
// will fail. This flag is set by the code signing API and cannot be set explicitly.
#define kSecCodeSignatureAdhoc 0x0002 /* must be used without signer */

// Implicitly set the "hard" status bit for the code when it starts running.
// This bit indicates that the code prefers to be denied access to a resource
// if gaining such access would cause its invalidation. Since the hard bit is
// sticky, setting this option bit guarantees that the code will always have
// it set.
#define kSecCodeSignatureForceHard 0x0100 /* always set HARD mode on launch */

// Implicitly set the "kill" status bit for the code when it starts running.
// This bit indicates that the code wishes to be terminated with prejudice if
// it is ever invalidated. Since the kill bit is sticky, setting this option bit
// guarantees that the code will always be dynamically valid, since it will die
// immediately	if it becomes invalid.
#define kSecCodeSignatureForceKill 0x0200 /* always set KILL mode on launch */

// Forces the kSecCSConsiderExpiration flag on all validations of the code.
#define kSecCodeSignatureForceExpiration 0x0400 /* force certificate expiration checks */

#define kSecCodeSignatureRestrict 0x0800 /* restrict dyld loading */
#define kSecCodeSignatureEnforcement 0x1000 /* enforce code signing */
#define kSecCodeSignatureLibraryValidation 0x2000 /* library validation required */

// Instructs the kernel to apply runtime hardening policies as required by the
// hardened runtime version
#define kSecCodeSignatureRuntime 0x10000 /* apply runtime hardening policies */

// The code was automatically signed by the linker. This signature should be
// ignored in any new signing operation.
#define kSecCodeSignatureLinkerSigned 0x20000 /* identify that the signature was auto-generated by the linker*/

/*
 * ARM subtypes
 * https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/osfmk/mach/machine.h
 */
/* Capability bits used in the definition of cpu_type. */
#define CPU_ARCH_MASK           0xff000000      /* mask for architecture bits */
#define CPU_ARCH_ABI64          0x01000000      /* 64 bit ABI */
#define CPU_ARCH_ABI64_32       0x02000000      /* ABI for 64-bit hardware with 32-bit types; LP32 */

/* Machine types known by all. */
#define CPU_TYPE_ANY            ((cpu_type_t) -1)

#define CPU_TYPE_VAX            ((cpu_type_t) 1)
/* skip				((cpu_type_t) 2)	*/
/* skip				((cpu_type_t) 3)	*/
/* skip				((cpu_type_t) 4)	*/
/* skip				((cpu_type_t) 5)	*/
#define CPU_TYPE_MC680x0        ((cpu_type_t) 6)
#define CPU_TYPE_X86            ((cpu_type_t) 7)
#define CPU_TYPE_I386           CPU_TYPE_X86            /* compatibility */
#define CPU_TYPE_X86_64         (CPU_TYPE_X86 | CPU_ARCH_ABI64)

/* skip CPU_TYPE_MIPS		((cpu_type_t) 8)	*/
/* skip                         ((cpu_type_t) 9)	*/
#define CPU_TYPE_MC98000        ((cpu_type_t) 10)
#define CPU_TYPE_HPPA           ((cpu_type_t) 11)
#define CPU_TYPE_ARM            ((cpu_type_t) 12)
#define CPU_TYPE_ARM64          (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#define CPU_TYPE_ARM64_32       (CPU_TYPE_ARM | CPU_ARCH_ABI64_32)
#define CPU_TYPE_MC88000        ((cpu_type_t) 13)
#define CPU_TYPE_SPARC          ((cpu_type_t) 14)
#define CPU_TYPE_I860           ((cpu_type_t) 15)
/* skip	CPU_TYPE_ALPHA		((cpu_type_t) 16)	*/
/* skip				((cpu_type_t) 17)	*/
#define CPU_TYPE_POWERPC                ((cpu_type_t) 18)
#define CPU_TYPE_POWERPC64              (CPU_TYPE_POWERPC | CPU_ARCH_ABI64)
/* skip				((cpu_type_t) 19) */
/* skip				((cpu_type_t) 20) */
/* skip				((cpu_type_t) 21) */
/* skip				((cpu_type_t) 22) */
/* skip				((cpu_type_t) 23) */
/* skip				((cpu_type_t) 24) */

/*
 *	Machine subtypes (these are defined here, instead of in a machine
 *	dependent directory, so that any program can get all definitions
 *	regardless of where is it compiled).
 */

/*
 * Capability bits used in the definition of cpu_subtype.
 */
#define CPU_SUBTYPE_MASK        0xff000000      /* mask for feature flags */
#define CPU_SUBTYPE_LIB64       0x80000000      /* 64 bit libraries */
#define CPU_SUBTYPE_PTRAUTH_ABI 0x80000000      /* pointer authentication with versioned ABI */

/*
 *      When selecting a slice, ANY will pick the slice with the best
 *      grading for the selected cpu_type_t, unlike the "ALL" subtypes,
 *      which are the slices that can run on any hardware for that cpu type.
 */
#define CPU_SUBTYPE_ANY         ((cpu_subtype_t) -1)

/*
 *	Object files that are hand-crafted to run on any
 *	implementation of an architecture are tagged with
 *	CPU_SUBTYPE_MULTIPLE.  This functions essentially the same as
 *	the "ALL" subtype of an architecture except that it allows us
 *	to easily find object files that may need to be modified
 *	whenever a new implementation of an architecture comes out.
 *
 *	It is the responsibility of the implementor to make sure the
 *	software handles unsupported implementations elegantly.
 */
#define CPU_SUBTYPE_MULTIPLE            ((cpu_subtype_t) -1)
#define CPU_SUBTYPE_LITTLE_ENDIAN       ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_BIG_ENDIAN          ((cpu_subtype_t) 1)

/*
 *     Machine threadtypes.
 *     This is none - not defined - for most machine types/subtypes.
 */
#define CPU_THREADTYPE_NONE             ((cpu_threadtype_t) 0)

/*
 *	VAX subtypes (these do *not* necessary conform to the actual cpu
 *	ID assigned by DEC available via the SID register).
 */

#define CPU_SUBTYPE_VAX_ALL     ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_VAX780      ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_VAX785      ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_VAX750      ((cpu_subtype_t) 3)
#define CPU_SUBTYPE_VAX730      ((cpu_subtype_t) 4)
#define CPU_SUBTYPE_UVAXI       ((cpu_subtype_t) 5)
#define CPU_SUBTYPE_UVAXII      ((cpu_subtype_t) 6)
#define CPU_SUBTYPE_VAX8200     ((cpu_subtype_t) 7)
#define CPU_SUBTYPE_VAX8500     ((cpu_subtype_t) 8)
#define CPU_SUBTYPE_VAX8600     ((cpu_subtype_t) 9)
#define CPU_SUBTYPE_VAX8650     ((cpu_subtype_t) 10)
#define CPU_SUBTYPE_VAX8800     ((cpu_subtype_t) 11)
#define CPU_SUBTYPE_UVAXIII     ((cpu_subtype_t) 12)

/*
 *      680x0 subtypes
 *
 * The subtype definitions here are unusual for historical reasons.
 * NeXT used to consider 68030 code as generic 68000 code.  For
 * backwards compatability:
 *
 *	CPU_SUBTYPE_MC68030 symbol has been preserved for source code
 *	compatability.
 *
 *	CPU_SUBTYPE_MC680x0_ALL has been defined to be the same
 *	subtype as CPU_SUBTYPE_MC68030 for binary comatability.
 *
 *	CPU_SUBTYPE_MC68030_ONLY has been added to allow new object
 *	files to be tagged as containing 68030-specific instructions.
 */

#define CPU_SUBTYPE_MC680x0_ALL         ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MC68030             ((cpu_subtype_t) 1) /* compat */
#define CPU_SUBTYPE_MC68040             ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MC68030_ONLY        ((cpu_subtype_t) 3)

/*
 *	I386 subtypes
 */

#define CPU_SUBTYPE_INTEL(f, m) ((cpu_subtype_t) (f) + ((m) << 4))

#define CPU_SUBTYPE_I386_ALL                    CPU_SUBTYPE_INTEL(3, 0)
#define CPU_SUBTYPE_386                                 CPU_SUBTYPE_INTEL(3, 0)
#define CPU_SUBTYPE_486                                 CPU_SUBTYPE_INTEL(4, 0)
#define CPU_SUBTYPE_486SX                               CPU_SUBTYPE_INTEL(4, 8) // 8 << 4 = 128
#define CPU_SUBTYPE_586                                 CPU_SUBTYPE_INTEL(5, 0)
#define CPU_SUBTYPE_PENT        CPU_SUBTYPE_INTEL(5, 0)
#define CPU_SUBTYPE_PENTPRO     CPU_SUBTYPE_INTEL(6, 1)
#define CPU_SUBTYPE_PENTII_M3   CPU_SUBTYPE_INTEL(6, 3)
#define CPU_SUBTYPE_PENTII_M5   CPU_SUBTYPE_INTEL(6, 5)
#define CPU_SUBTYPE_CELERON                             CPU_SUBTYPE_INTEL(7, 6)
#define CPU_SUBTYPE_CELERON_MOBILE              CPU_SUBTYPE_INTEL(7, 7)
#define CPU_SUBTYPE_PENTIUM_3                   CPU_SUBTYPE_INTEL(8, 0)
#define CPU_SUBTYPE_PENTIUM_3_M                 CPU_SUBTYPE_INTEL(8, 1)
#define CPU_SUBTYPE_PENTIUM_3_XEON              CPU_SUBTYPE_INTEL(8, 2)
#define CPU_SUBTYPE_PENTIUM_M                   CPU_SUBTYPE_INTEL(9, 0)
#define CPU_SUBTYPE_PENTIUM_4                   CPU_SUBTYPE_INTEL(10, 0)
#define CPU_SUBTYPE_PENTIUM_4_M                 CPU_SUBTYPE_INTEL(10, 1)
#define CPU_SUBTYPE_ITANIUM                             CPU_SUBTYPE_INTEL(11, 0)
#define CPU_SUBTYPE_ITANIUM_2                   CPU_SUBTYPE_INTEL(11, 1)
#define CPU_SUBTYPE_XEON                                CPU_SUBTYPE_INTEL(12, 0)
#define CPU_SUBTYPE_XEON_MP                             CPU_SUBTYPE_INTEL(12, 1)

#define CPU_SUBTYPE_INTEL_FAMILY(x)     ((x) & 15)
#define CPU_SUBTYPE_INTEL_FAMILY_MAX    15

#define CPU_SUBTYPE_INTEL_MODEL(x)      ((x) >> 4)
#define CPU_SUBTYPE_INTEL_MODEL_ALL     0

/*
 *	X86 subtypes.
 */

#define CPU_SUBTYPE_X86_ALL             ((cpu_subtype_t)3)
#define CPU_SUBTYPE_X86_64_ALL          ((cpu_subtype_t)3 | CPU_SUBTYPE_LIB64)
#define CPU_SUBTYPE_X86_ARCH1           ((cpu_subtype_t)4)
#define CPU_SUBTYPE_X86_64_H            ((cpu_subtype_t)8)      /* Haswell feature subset */


#define CPU_THREADTYPE_INTEL_HTT        ((cpu_threadtype_t) 1)

/*
 *	Mips subtypes.
 */

#define CPU_SUBTYPE_MIPS_ALL    ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MIPS_R2300  ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MIPS_R2600  ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MIPS_R2800  ((cpu_subtype_t) 3)
#define CPU_SUBTYPE_MIPS_R2000a ((cpu_subtype_t) 4)     /* pmax */
#define CPU_SUBTYPE_MIPS_R2000  ((cpu_subtype_t) 5)
#define CPU_SUBTYPE_MIPS_R3000a ((cpu_subtype_t) 6)     /* 3max */
#define CPU_SUBTYPE_MIPS_R3000  ((cpu_subtype_t) 7)

/*
 *	MC98000 (PowerPC) subtypes
 */
#define CPU_SUBTYPE_MC98000_ALL ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MC98601     ((cpu_subtype_t) 1)

/*
 *	HPPA subtypes for Hewlett-Packard HP-PA family of
 *	risc processors. Port by NeXT to 700 series.
 */

#define CPU_SUBTYPE_HPPA_ALL            ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_HPPA_7100           ((cpu_subtype_t) 0) /* compat */
#define CPU_SUBTYPE_HPPA_7100LC         ((cpu_subtype_t) 1)

/*
 *	MC88000 subtypes.
 */
#define CPU_SUBTYPE_MC88000_ALL ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MC88100     ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MC88110     ((cpu_subtype_t) 2)

/*
 *	SPARC subtypes
 */
#define CPU_SUBTYPE_SPARC_ALL           ((cpu_subtype_t) 0)

/*
 *	I860 subtypes
 */
#define CPU_SUBTYPE_I860_ALL    ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_I860_860    ((cpu_subtype_t) 1)

/*
 *	PowerPC subtypes
 */
#define CPU_SUBTYPE_POWERPC_ALL         ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_POWERPC_601         ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_POWERPC_602         ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_POWERPC_603         ((cpu_subtype_t) 3)
#define CPU_SUBTYPE_POWERPC_603e        ((cpu_subtype_t) 4)
#define CPU_SUBTYPE_POWERPC_603ev       ((cpu_subtype_t) 5)
#define CPU_SUBTYPE_POWERPC_604         ((cpu_subtype_t) 6)
#define CPU_SUBTYPE_POWERPC_604e        ((cpu_subtype_t) 7)
#define CPU_SUBTYPE_POWERPC_620         ((cpu_subtype_t) 8)
#define CPU_SUBTYPE_POWERPC_750         ((cpu_subtype_t) 9)
#define CPU_SUBTYPE_POWERPC_7400        ((cpu_subtype_t) 10)
#define CPU_SUBTYPE_POWERPC_7450        ((cpu_subtype_t) 11)
#define CPU_SUBTYPE_POWERPC_970         ((cpu_subtype_t) 100)

/*
 *	ARM subtypes
 */
#define CPU_SUBTYPE_ARM_ALL             ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM_V4T             ((cpu_subtype_t) 5)
#define CPU_SUBTYPE_ARM_V6              ((cpu_subtype_t) 6)
#define CPU_SUBTYPE_ARM_V5TEJ           ((cpu_subtype_t) 7)
#define CPU_SUBTYPE_ARM_XSCALE          ((cpu_subtype_t) 8)
#define CPU_SUBTYPE_ARM_V7              ((cpu_subtype_t) 9)  /* ARMv7-A and ARMv7-R */
#define CPU_SUBTYPE_ARM_V7F             ((cpu_subtype_t) 10) /* Cortex A9 */
#define CPU_SUBTYPE_ARM_V7S             ((cpu_subtype_t) 11) /* Swift */
#define CPU_SUBTYPE_ARM_V7K             ((cpu_subtype_t) 12)
#define CPU_SUBTYPE_ARM_V8              ((cpu_subtype_t) 13)
#define CPU_SUBTYPE_ARM_V6M             ((cpu_subtype_t) 14) /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V7M             ((cpu_subtype_t) 15) /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V7EM            ((cpu_subtype_t) 16) /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V8M             ((cpu_subtype_t) 17) /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V8M_MAIN        CPU_SUBTYPE_ARM_V8M  /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V8M_BASE        ((cpu_subtype_t) 18) /* Not meant to be run under xnu */
#define CPU_SUBTYPE_ARM_V8_1M_MAIN      ((cpu_subtype_t) 19) /* Not meant to be run under xnu */

/*
 *  ARM64 subtypes
 */
#define CPU_SUBTYPE_ARM64_ALL           ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM64_V8            ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_ARM64E              ((cpu_subtype_t) 2)

/* CPU subtype feature flags for ptrauth on arm64e platforms */
#define CPU_SUBTYPE_ARM64_PTR_AUTH_MASK 0x0f000000
#define CPU_SUBTYPE_ARM64_PTR_AUTH_VERSION(x) (((x) & CPU_SUBTYPE_ARM64_PTR_AUTH_MASK) >> 24)
// #ifdef PRIVATE
#define CPU_SUBTYPE_ARM64_PTR_AUTHV0_VERSION 0
#define CPU_SUBTYPE_ARM64_PTR_AUTHV1_VERSION 1
#define CPU_SUBTYPE_ARM64_PTR_AUTH_CURRENT_VERSION CPU_SUBTYPE_ARM64_PTR_AUTHV0_VERSION
// #if XNU_TARGET_OS_OSX
// #define CPU_SUBTYPE_ARM64_PTR_AUTH_MAX_PREFERRED_VERSION CPU_SUBTYPE_ARM64_PTR_AUTHV1_VERSION
// #else /* XNU_TARGET_OS_OSX */
// #define CPU_SUBTYPE_ARM64_PTR_AUTH_MAX_PREFERRED_VERSION CPU_SUBTYPE_ARM64_PTR_AUTHV0_VERSION
// #endif /* XNU_TARGET_OS_OSX */
// #endif /* PRIVATE */

/*
 *  ARM64_32 subtypes
 */
#define CPU_SUBTYPE_ARM64_32_ALL        ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM64_32_V8 ((cpu_subtype_t) 1)

/*
 *	CPU families (sysctl hw.cpufamily)
 *
 * These are meant to identify the CPU's marketing name - an
 * application can map these to (possibly) localized strings.
 * NB: the encodings of the CPU families are intentionally arbitrary.
 * There is no ordering, and you should never try to deduce whether
 * or not some feature is available based on the family.
 * Use feature flags (eg, hw.optional.altivec) to test for optional
 * functionality.
 */
#define CPUFAMILY_UNKNOWN               0
#define CPUFAMILY_POWERPC_G3            0xcee41549
#define CPUFAMILY_POWERPC_G4            0x77c184ae
#define CPUFAMILY_POWERPC_G5            0xed76d8aa
#define CPUFAMILY_INTEL_6_13            0xaa33392b
#define CPUFAMILY_INTEL_PENRYN          0x78ea4fbc
#define CPUFAMILY_INTEL_NEHALEM         0x6b5a4cd2
#define CPUFAMILY_INTEL_WESTMERE        0x573b5eec
#define CPUFAMILY_INTEL_SANDYBRIDGE     0x5490b78c
#define CPUFAMILY_INTEL_IVYBRIDGE       0x1f65e835
#define CPUFAMILY_INTEL_HASWELL         0x10b282dc
#define CPUFAMILY_INTEL_BROADWELL       0x582ed09c
#define CPUFAMILY_INTEL_SKYLAKE         0x37fc219f
#define CPUFAMILY_INTEL_KABYLAKE        0x0f817246
#define CPUFAMILY_INTEL_ICELAKE         0x38435547
#define CPUFAMILY_INTEL_COMETLAKE       0x1cf8a03e
#define CPUFAMILY_ARM_9                 0xe73283ae
#define CPUFAMILY_ARM_11                0x8ff620d8
#define CPUFAMILY_ARM_XSCALE            0x53b005f5
#define CPUFAMILY_ARM_12                0xbd1b0ae9
#define CPUFAMILY_ARM_13                0x0cc90e64
#define CPUFAMILY_ARM_14                0x96077ef1
#define CPUFAMILY_ARM_15                0xa8511bca
#define CPUFAMILY_ARM_SWIFT             0x1e2d6381
#define CPUFAMILY_ARM_CYCLONE           0x37a09642
#define CPUFAMILY_ARM_TYPHOON           0x2c91a47e
#define CPUFAMILY_ARM_TWISTER           0x92fb37c8
#define CPUFAMILY_ARM_HURRICANE         0x67ceee93
#define CPUFAMILY_ARM_MONSOON_MISTRAL   0xe81e7ef6
#define CPUFAMILY_ARM_VORTEX_TEMPEST    0x07d34b9f
#define CPUFAMILY_ARM_LIGHTNING_THUNDER 0x462504d2
#define CPUFAMILY_ARM_FIRESTORM_ICESTORM 0x1b588bb3
#define CPUFAMILY_ARM_BLIZZARD_AVALANCHE 0xda33d83d
#define CPUFAMILY_ARM_EVEREST_SAWTOOTH  0x8765edea
#define CPUFAMILY_ARM_IBIZA             0xfa33415e
#define CPUFAMILY_ARM_PALMA 0x72015832
#define CPUFAMILY_ARM_COLL 0x2876f5b5
#define CPUFAMILY_ARM_LOBOS 0x5f4dea93
#define CPUFAMILY_ARM_DONAN 0x6f5129ac
#define CPUFAMILY_ARM_BRAVA 0x17d5b93a
#define CPUFAMILY_ARM_TAHITI 0x75d4acb9
#define CPUFAMILY_ARM_TUPAI 0x204526d0
#define CPUFAMILY_ARM_HIDRA 0x1d5a87e8
#define CPUFAMILY_ARM_SOTRA 0xf76c5b1a
#define CPUFAMILY_ARM_THERA 0xab345f09
#define CPUFAMILY_ARM_TILOS 0x01d7a72b

/* Described in rdar://64125549 */
#define CPUSUBFAMILY_UNKNOWN            0
#define CPUSUBFAMILY_ARM_HP             1
#define CPUSUBFAMILY_ARM_HG             2
#define CPUSUBFAMILY_ARM_M              3
#define CPUSUBFAMILY_ARM_HS             4
#define CPUSUBFAMILY_ARM_HC_HD          5
#define CPUSUBFAMILY_ARM_HA             6

/* The following synonyms are deprecated: */
#define CPUFAMILY_INTEL_6_23    CPUFAMILY_INTEL_PENRYN
#define CPUFAMILY_INTEL_6_26    CPUFAMILY_INTEL_NEHALEM

/*
 * x86 Thread Flavors
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/mach/i386/thread_status.h#L93-L180
 */
#define i386_THREAD_STATE               1
#define i386_FLOAT_STATE                2
#define i386_EXCEPTION_STATE            3
#define x86_THREAD_STATE32              1
#define x86_FLOAT_STATE32               2
#define x86_EXCEPTION_STATE32           3
#define x86_THREAD_STATE64              4
#define x86_FLOAT_STATE64               5
#define x86_EXCEPTION_STATE64           6
#define x86_THREAD_STATE                7
#define x86_FLOAT_STATE                 8
#define x86_EXCEPTION_STATE             9
#define x86_DEBUG_STATE32               10
#define x86_DEBUG_STATE64               11
#define x86_DEBUG_STATE                 12
#define x86_THREAD_STATE_NONE           13
#define x86_SAVED_STATE32               14 /* internal */
#define x86_SAVED_STATE64               15 /* internal */
/* Arrange for flavors to take sequential values, 32-bit, 64-bit, non-specific */
#define x86_AVX_STATE32                 16
#define x86_AVX_STATE64                 (x86_AVX_STATE32 + 1)
#define x86_AVX_STATE                   (x86_AVX_STATE32 + 2)
#define x86_AVX512_STATE32              19
#define x86_AVX512_STATE64              (x86_AVX512_STATE32 + 1)
#define x86_AVX512_STATE                (x86_AVX512_STATE32 + 2)
#define x86_PAGEIN_STATE                22
#define x86_THREAD_FULL_STATE64         23
#define x86_INSTRUCTION_STATE           24
#define x86_LAST_BRANCH_STATE           25
#define x86_THREAD_STATE_FLAVORS        26  /* This must be updated to 1 more than the highest numerical state flavor */

/* Size of maximum exported thread state in 32-bit words */
#define I386_THREAD_STATE_MAX           614 /* Size of biggest state possible */

#define x86_FLAVOR_MODIFIES_CORE_CPU_REGISTERS(x) \
((x == x86_THREAD_STATE) ||     \
 (x == x86_THREAD_STATE32) ||   \
 (x == x86_THREAD_STATE64) ||   \
 (x == x86_THREAD_FULL_STATE64))

/*
 * x86_VALID_THREAD_STATE_FLAVOR is a platform specific macro that when passed
 * an exception flavor will return if that is a defined flavor for that
 * platform. The macro must be manually updated to include all of the valid
 * exception flavors as defined above.
 */
#define x86_VALID_THREAD_STATE_FLAVOR(x) \
	 ((x == x86_THREAD_STATE32)		|| \
	  (x == x86_FLOAT_STATE32)		|| \
	  (x == x86_EXCEPTION_STATE32)		|| \
	  (x == x86_DEBUG_STATE32)		|| \
	  (x == x86_THREAD_STATE64)		|| \
	  (x == x86_THREAD_FULL_STATE64)	|| \
	  (x == x86_FLOAT_STATE64)		|| \
	  (x == x86_EXCEPTION_STATE64)		|| \
	  (x == x86_DEBUG_STATE64)		|| \
	  (x == x86_THREAD_STATE)		|| \
	  (x == x86_FLOAT_STATE)		|| \
	  (x == x86_EXCEPTION_STATE)		|| \
	  (x == x86_DEBUG_STATE)		|| \
	  (x == x86_AVX_STATE32)		|| \
	  (x == x86_AVX_STATE64)		|| \
	  (x == x86_AVX_STATE)			|| \
	  (x == x86_AVX512_STATE32)		|| \
	  (x == x86_AVX512_STATE64)		|| \
	  (x == x86_AVX512_STATE)		|| \
	  (x == x86_PAGEIN_STATE)		|| \
	  (x == x86_INSTRUCTION_STATE)		|| \
	  (x == x86_LAST_BRANCH_STATE)		|| \
	  (x == x86_THREAD_STATE_NONE))

struct x86_state_hdr {
	uint32_t        flavor;
	uint32_t        count;
};
typedef struct x86_state_hdr x86_state_hdr_t;

// Thread states, used in LC_UNIXTHREAD load command.
// Reference:
// https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/mach/i386/_structs.h#L713-L765
struct x86_mmst_reg
{
    uint8_t    mmst_reg[10];
    uint8_t    mmst_rsrv[6];
};

struct x86_thread_state32 {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ss;
    uint32_t eflags;
    uint32_t eip;
    uint32_t cs;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
};
typedef struct x86_thread_state32 x86_thread_state32_t;
#define i386_THREAD_STATE_COUNT ((uint32_t) (sizeof (struct x86_thread_state32) / sizeof (uint32_t)))
#define x86_THREAD_STATE32_COUNT i386_THREAD_STATE_COUNT

struct x86_float_state32 {
	uint32_t            fpu_reserved[2];
	uint16_t            fpu_fcw;          /* x87 FPU control word */
	uint16_t            fpu_fsw;          /* x87 FPU status word */
	uint8_t             fpu_ftw;          /* x87 FPU tag word */
	uint8_t             fpu_rsrv1;        /* reserved */ 
	uint16_t            fpu_fop;          /* x87 FPU Opcode */
	uint32_t            fpu_ip;           /* x87 FPU Instruction Pointer offset */
	uint16_t            fpu_cs;           /* x87 FPU Instruction Pointer Selector */
	uint16_t            fpu_rsrv2;        /* reserved */
	uint32_t            fpu_dp;           /* x87 FPU Instruction Operand(Data) Pointer offset */
	uint16_t            fpu_ds;           /* x87 FPU Instruction Operand(Data) Pointer Selector */
	uint16_t            fpu_rsrv3;		  /* reserved */
	uint32_t            fpu_mxcsr;		  /* MXCSR Register state */
	uint32_t            fpu_mxcsrmask;    /* MXCSR mask */
	struct x86_mmst_reg fpu_stmm0;        /* ST0/MM0    */
	struct x86_mmst_reg fpu_stmm1;        /* ST1/MM1    */
	struct x86_mmst_reg fpu_stmm2;        /* ST2/MM2    */
	struct x86_mmst_reg fpu_stmm3;        /* ST3/MM3    */
	struct x86_mmst_reg fpu_stmm4;        /* ST4/MM4    */
	struct x86_mmst_reg fpu_stmm5;        /* ST5/MM5    */
	struct x86_mmst_reg fpu_stmm6;        /* ST6/MM6    */
	struct x86_mmst_reg fpu_stmm7;        /* ST7/MM7    */
	uint8_t             fpu_xmm0[16];     /* XMM 0      */
	uint8_t             fpu_xmm1[16];     /* XMM 1      */
	uint8_t             fpu_xmm2[16];     /* XMM 2      */
	uint8_t             fpu_xmm3[16];     /* XMM 3      */
	uint8_t             fpu_xmm4[16];     /* XMM 4      */
	uint8_t             fpu_xmm5[16];     /* XMM 5      */
	uint8_t             fpu_xmm6[16];     /* XMM 6      */
	uint8_t             fpu_xmm7[16];     /* XMM 7      */
	uint8_t			    fpu_rsrv4[14*16]; /* reserved   */
	uint32_t            fpu_reserved1;
};
typedef struct x86_float_state32 x86_float_state32_t;
#define i386_FLOAT_STATE_COUNT ((uint32_t) (sizeof (struct x86_float_state32) / sizeof (uint32_t)))
#define x86_FLOAT_STATE32_COUNT i386_FLOAT_STATE_COUNT

struct x86_avx_state32
{
    uint32_t            fpu_reserved[2];
    uint16_t            fpu_fcw;          /* x87 FPU control word */
    uint16_t            fpu_fsw;          /* x87 FPU status word */
    uint8_t             fpu_ftw;          /* x87 FPU tag word */
    uint8_t             fpu_rsrv1;        /* reserved */ 
    uint16_t            fpu_fop;          /* x87 FPU Opcode */
    uint32_t            fpu_ip;           /* x87 FPU Instruction Pointer offset */
    uint16_t            fpu_cs;           /* x87 FPU Instruction Pointer Selector */
    uint16_t            fpu_rsrv2;        /* reserved */
    uint32_t            fpu_dp;           /* x87 FPU Instruction Operand(Data) Pointer offset */
    uint16_t            fpu_ds;           /* x87 FPU Instruction Operand(Data) Pointer Selector */
    uint16_t            fpu_rsrv3;        /* reserved */
    uint32_t            fpu_mxcsr;        /* MXCSR Register state */
    uint32_t            fpu_mxcsrmask;    /* MXCSR mask */
    struct x86_mmst_reg fpu_stmm0;        /* ST0/MM0   */
    struct x86_mmst_reg fpu_stmm1;        /* ST1/MM1  */
    struct x86_mmst_reg fpu_stmm2;        /* ST2/MM2  */
    struct x86_mmst_reg fpu_stmm3;        /* ST3/MM3  */
    struct x86_mmst_reg fpu_stmm4;        /* ST4/MM4  */
    struct x86_mmst_reg fpu_stmm5;        /* ST5/MM5  */
    struct x86_mmst_reg fpu_stmm6;        /* ST6/MM6  */
    struct x86_mmst_reg fpu_stmm7;        /* ST7/MM7  */
    uint8_t             fpu_xmm0[16];     /* XMM 0  */
    uint8_t             fpu_xmm1[16];     /* XMM 1  */
    uint8_t             fpu_xmm2[16];     /* XMM 2  */
    uint8_t             fpu_xmm3[16];     /* XMM 3  */
    uint8_t             fpu_xmm4[16];     /* XMM 4  */
    uint8_t             fpu_xmm5[16];     /* XMM 5  */
    uint8_t             fpu_xmm6[16];     /* XMM 6  */
    uint8_t             fpu_xmm7[16];     /* XMM 7  */
    uint8_t			    fpu_rsrv4[14*16]; /* reserved */
    int32_t             fpu_reserved1;
    uint8_t			    avx_reserved1[64];
    uint8_t             fpu_ymmh0[16];    /* YMMH 0  */
    uint8_t             fpu_ymmh1[16];    /* YMMH 1  */
    uint8_t             fpu_ymmh2[16];    /* YMMH 2  */
    uint8_t             fpu_ymmh3[16];    /* YMMH 3  */
    uint8_t             fpu_ymmh4[16];    /* YMMH 4  */
    uint8_t             fpu_ymmh5[16];    /* YMMH 5  */
    uint8_t             fpu_ymmh6[16];    /* YMMH 6  */
    uint8_t             fpu_ymmh7[16];    /* YMMH 7  */
};
typedef struct x86_avx_state32 x86_avx_state32_t;
#define x86_AVX_STATE32_COUNT ((uint32_t) (sizeof (struct x86_avx_state32) / sizeof (uint32_t)))

struct x86_avx512_state32
{
    uint32_t            fpu_reserved[2];
    uint16_t            fpu_fcw;            /* x87 FPU control word */
    uint16_t            fpu_fsw;            /* x87 FPU status word */
    uint8_t             fpu_ftw;            /* x87 FPU tag word */
    uint8_t             fpu_rsrv1;          /* reserved */ 
    uint16_t            fpu_fop;            /* x87 FPU Opcode */
    uint32_t            fpu_ip;             /* x87 FPU Instruction Pointer offset */
    uint16_t            fpu_cs;             /* x87 FPU Instruction Pointer Selector */
    uint16_t            fpu_rsrv2;          /* reserved */
    uint32_t            fpu_dp;             /* x87 FPU Instruction Operand(Data) Pointer offset */
    uint16_t            fpu_ds;             /* x87 FPU Instruction Operand(Data) Pointer Selector */
    uint16_t            fpu_rsrv3;          /* reserved */
    uint32_t            fpu_mxcsr;          /* MXCSR Register state */
    uint32_t            fpu_mxcsrmask;      /* MXCSR mask */
    struct x86_mmst_reg fpu_stmm0;          /* ST0/MM0   */
    struct x86_mmst_reg fpu_stmm1;          /* ST1/MM1  */
    struct x86_mmst_reg fpu_stmm2;          /* ST2/MM2  */
    struct x86_mmst_reg fpu_stmm3;          /* ST3/MM3  */
    struct x86_mmst_reg fpu_stmm4;          /* ST4/MM4  */
    struct x86_mmst_reg fpu_stmm5;          /* ST5/MM5  */
    struct x86_mmst_reg fpu_stmm6;          /* ST6/MM6  */
    struct x86_mmst_reg fpu_stmm7;          /* ST7/MM7  */
    uint8_t             fpu_xmm0[16];       /* XMM 0  */
    uint8_t             fpu_xmm1[16];       /* XMM 1  */
    uint8_t             fpu_xmm2[16];       /* XMM 2  */
    uint8_t             fpu_xmm3[16];       /* XMM 3  */
    uint8_t             fpu_xmm4[16];       /* XMM 4  */
    uint8_t             fpu_xmm5[16];       /* XMM 5  */
    uint8_t             fpu_xmm6[16];       /* XMM 6  */
    uint8_t             fpu_xmm7[16];       /* XMM 7  */
    int8_t              fpu_rsrv4[14*16];   /* reserved */
    int32_t             fpu_reserved1;
    int8_t              avx_reserved1[64];
    uint8_t             fpu_ymmh0[16];		/* YMMH 0  */
    uint8_t             fpu_ymmh1[16];		/* YMMH 1  */
    uint8_t             fpu_ymmh2[16];		/* YMMH 2  */
    uint8_t             fpu_ymmh3[16];		/* YMMH 3  */
    uint8_t             fpu_ymmh4[16];		/* YMMH 4  */
    uint8_t             fpu_ymmh5[16];		/* YMMH 5  */
    uint8_t             fpu_ymmh6[16];		/* YMMH 6  */
    uint8_t             fpu_ymmh7[16];		/* YMMH 7  */
    uint8_t             fpu_k0[8];          /* K0 */
    uint8_t             fpu_k1[8];          /* K1 */
    uint8_t             fpu_k2[8];          /* K2 */
    uint8_t             fpu_k3[8];          /* K3 */
    uint8_t             fpu_k4[8];          /* K4 */
    uint8_t             fpu_k5[8];          /* K5 */
    uint8_t             fpu_k6[8];          /* K6 */
    uint8_t             fpu_k7[8];          /* K7 */
    uint8_t             fpu_zmmh0[32];      /* ZMMH 0  */
    uint8_t             fpu_zmmh1[32];      /* ZMMH 1  */
    uint8_t             fpu_zmmh2[32];      /* ZMMH 2  */
    uint8_t             fpu_zmmh3[32];      /* ZMMH 3  */
    uint8_t             fpu_zmmh4[32];      /* ZMMH 4  */
    uint8_t             fpu_zmmh5[32];      /* ZMMH 5  */
    uint8_t             fpu_zmmh6[32];      /* ZMMH 6  */
    uint8_t             fpu_zmmh7[32];      /* ZMMH 7  */
};
typedef struct x86_avx512_state32 x86_avx512_state32_t;
#define x86_AVX512_STATE32_COUNT ((uint32_t) (sizeof (struct x86_avx512_state32) / sizeof (uint32_t)))

struct x86_exception_state32
{
    uint16_t trapno;
    uint16_t cpu;
    uint32_t err;
    uint32_t faultvaddr;
};
typedef struct x86_exception_state32 x86_exception_state32_t;
#define i386_EXCEPTION_STATE_COUNT ((uint32_t) (sizeof (struct x86_exception_state32) / sizeof (uint32_t)))
#define x86_EXCEPTION_STATE32_COUNT i386_EXCEPTION_STATE_COUNT

struct x86_debug_state32
{
    uint32_t dr0;
    uint32_t dr1;
    uint32_t dr2;
    uint32_t dr3;
    uint32_t dr4;
    uint32_t dr5;
    uint32_t dr6;
    uint32_t dr7;
};
typedef struct x86_debug_state32 x86_debug_state32_t;
#define x86_DEBUG_STATE32_COUNT ((uint32_t) (sizeof (struct x86_debug_state32) / sizeof (uint32_t)))

struct x86_instruction_state
{
    int32_t insn_stream_valid_bytes;
    int32_t insn_offset;
    int32_t out_of_synch;
    /*
     * non-zero when the cacheline that includes the insn_offset
     * is replaced in the insn_bytes array due to a mismatch
     * detected when comparing it with the same cacheline in memory
     */
#define x86_INSTRUCTION_STATE_MAX_INSN_BYTES    (2448 - 64 - 4)
    uint8_t	insn_bytes[x86_INSTRUCTION_STATE_MAX_INSN_BYTES];
#define x86_INSTRUCTION_STATE_CACHELINE_SIZE	64
    uint8_t	insn_cacheline[x86_INSTRUCTION_STATE_CACHELINE_SIZE];
};
typedef struct x86_instruction_state x86_instruction_state_t;
#define x86_INSTRUCTION_STATE_COUNT ((uint32_t) (sizeof (struct x86_instruction_state) / sizeof (uint32_t)))

/*
 * x86_64 bit versions
 */
struct x86_thread_state64
{
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rsp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rip;
	uint64_t rflags;
	uint64_t cs;
	uint64_t fs;
	uint64_t gs;
};
typedef struct x86_thread_state64 x86_thread_state64_t;
#define x86_THREAD_STATE64_COUNT ((uint32_t) (sizeof (struct x86_thread_state64) / sizeof (uint32_t)))

struct x86_thread_full_state64
{
    struct x86_thread_state64 ss64;
    uint64_t                  ds;
    uint64_t                  es;
    uint64_t                  ss;
    uint64_t                  gsbase;
};
typedef struct x86_thread_full_state64 x86_thread_full_state64_t;
#define x86_THREAD_FULL_STATE64_COUNT ((uint32_t) (sizeof (struct x86_thread_full_state64) / sizeof (uint32_t)))

struct x86_float_state64
{
    int32_t             fpu_reserved[2];
    uint16_t            fpu_fcw;         /* x87 FPU control word */
    uint16_t            fpu_fsw;         /* x87 FPU status word */
    uint8_t             fpu_ftw;         /* x87 FPU tag word */
    uint8_t             fpu_rsrv1;       /* reserved */ 
    uint16_t            fpu_fop;         /* x87 FPU Opcode */

    /* x87 FPU Instruction Pointer */
    uint32_t            fpu_ip;          /* offset */
    uint16_t            fpu_cs;          /* Selector */

    uint16_t            fpu_rsrv2;       /* reserved */

    /* x87 FPU Instruction Operand(Data) Pointer */
    uint32_t            fpu_dp;          /* offset */
    uint16_t            fpu_ds;          /* Selector */

    uint16_t            fpu_rsrv3;       /* reserved */
    uint32_t            fpu_mxcsr;       /* MXCSR Register state */
    uint32_t            fpu_mxcsrmask;   /* MXCSR mask */
    struct x86_mmst_reg fpu_stmm0;       /* ST0/MM0   */
    struct x86_mmst_reg fpu_stmm1;       /* ST1/MM1  */
    struct x86_mmst_reg fpu_stmm2;       /* ST2/MM2  */
    struct x86_mmst_reg fpu_stmm3;       /* ST3/MM3  */
    struct x86_mmst_reg fpu_stmm4;       /* ST4/MM4  */
    struct x86_mmst_reg fpu_stmm5;       /* ST5/MM5  */
    struct x86_mmst_reg fpu_stmm6;       /* ST6/MM6  */
    struct x86_mmst_reg fpu_stmm7;       /* ST7/MM7  */
    uint8_t             fpu_xmm0[16];    /* XMM 0  */
    uint8_t             fpu_xmm1[16];    /* XMM 1  */
    uint8_t             fpu_xmm2[16];    /* XMM 2  */
    uint8_t             fpu_xmm3[16];    /* XMM 3  */
    uint8_t             fpu_xmm4[16];    /* XMM 4  */
    uint8_t             fpu_xmm5[16];    /* XMM 5  */
    uint8_t             fpu_xmm6[16];    /* XMM 6  */
    uint8_t             fpu_xmm7[16];    /* XMM 7  */
    uint8_t             fpu_xmm8[16];    /* XMM 8  */
    uint8_t             fpu_xmm9[16];    /* XMM 9  */
    uint8_t             fpu_xmm10[16];   /* XMM 10  */
    uint8_t             fpu_xmm11[16];   /* XMM 11 */
    uint8_t             fpu_xmm12[16];   /* XMM 12  */
    uint8_t             fpu_xmm13[16];   /* XMM 13  */
    uint8_t             fpu_xmm14[16];   /* XMM 14  */
    uint8_t             fpu_xmm15[16];   /* XMM 15  */
    int8_t              fpu_rsrv4[6*16]; /* reserved */
    int32_t             fpu_reserved1;
};
typedef struct x86_float_state64 x86_float_state64_t;
#define x86_FLOAT_STATE64_COUNT ((uint32_t) (sizeof (struct x86_float_state64) / sizeof (uint32_t)))

struct x86_avx_state64
{
    int32_t             fpu_reserved[2];
    uint64_t            fpu_fcw;         /* x87 FPU control word */
    uint64_t            fpu_fsw;         /* x87 FPU status word */
    uint8_t             fpu_ftw;         /* x87 FPU tag word */
    uint8_t             fpu_rsrv1;       /* reserved */ 
    uint16_t            fpu_fop;         /* x87 FPU Opcode */

    /* x87 FPU Instruction Pointer */
    uint32_t            fpu_ip;          /* offset */
    uint16_t            fpu_cs;          /* Selector */

    uint16_t            fpu_rsrv2;       /* reserved */

    /* x87 FPU Instruction Operand(Data) Pointer */
    uint32_t            fpu_dp;          /* offset */
    uint16_t            fpu_ds;          /* Selector */

    uint16_t            fpu_rsrv3;       /* reserved */
    uint32_t            fpu_mxcsr;       /* MXCSR Register state */
    uint32_t            fpu_mxcsrmask;   /* MXCSR mask */
    struct x86_mmst_reg fpu_stmm0;       /* ST0/MM0   */
    struct x86_mmst_reg fpu_stmm1;       /* ST1/MM1  */
    struct x86_mmst_reg fpu_stmm2;       /* ST2/MM2  */
    struct x86_mmst_reg fpu_stmm3;       /* ST3/MM3  */
    struct x86_mmst_reg fpu_stmm4;       /* ST4/MM4  */
    struct x86_mmst_reg fpu_stmm5;       /* ST5/MM5  */
    struct x86_mmst_reg fpu_stmm6;       /* ST6/MM6  */
    struct x86_mmst_reg fpu_stmm7;       /* ST7/MM7  */
    uint8_t             fpu_xmm0[16];    /* XMM 0  */
    uint8_t             fpu_xmm1[16];    /* XMM 1  */
    uint8_t             fpu_xmm2[16];    /* XMM 2  */
    uint8_t             fpu_xmm3[16];    /* XMM 3  */
    uint8_t             fpu_xmm4[16];    /* XMM 4  */
    uint8_t             fpu_xmm5[16];    /* XMM 5  */
    uint8_t             fpu_xmm6[16];    /* XMM 6  */
    uint8_t             fpu_xmm7[16];    /* XMM 7  */
    uint8_t             fpu_xmm8[16];    /* XMM 8  */
    uint8_t             fpu_xmm9[16];    /* XMM 9  */
    uint8_t             fpu_xmm10[16];   /* XMM 10  */
    uint8_t             fpu_xmm11[16];   /* XMM 11 */
    uint8_t             fpu_xmm12[16];   /* XMM 12  */
    uint8_t             fpu_xmm13[16];   /* XMM 13  */
    uint8_t             fpu_xmm14[16];   /* XMM 14  */
    uint8_t             fpu_xmm15[16];   /* XMM 15  */
    int8_t              fpu_rsrv4[6*16]; /* reserved */
    int32_t             fpu_reserved1;
    int8_t              avx_reserved1[64];
    uint8_t             fpu_ymmh0[16];   /* YMMH 0  */
    uint8_t             fpu_ymmh1[16];   /* YMMH 1  */
    uint8_t             fpu_ymmh2[16];   /* YMMH 2  */
    uint8_t             fpu_ymmh3[16];   /* YMMH 3  */
    uint8_t             fpu_ymmh4[16];   /* YMMH 4  */
    uint8_t             fpu_ymmh5[16];   /* YMMH 5  */
    uint8_t             fpu_ymmh6[16];   /* YMMH 6  */
    uint8_t             fpu_ymmh7[16];   /* YMMH 7  */
    uint8_t             fpu_ymmh8[16];   /* YMMH 8  */
    uint8_t             fpu_ymmh9[16];   /* YMMH 9  */
    uint8_t             fpu_ymmh10[16];  /* YMMH 10  */
    uint8_t             fpu_ymmh11[16];  /* YMMH 11  */
    uint8_t             fpu_ymmh12[16];  /* YMMH 12  */
    uint8_t             fpu_ymmh13[16];  /* YMMH 13  */
    uint8_t             fpu_ymmh14[16];  /* YMMH 14  */
    uint8_t             fpu_ymmh15[16];  /* YMMH 15  */
};
typedef struct x86_avx_state64 x86_avx_state64_t;
#define x86_AVX_STATE64_COUNT ((uint32_t) (sizeof (struct x86_avx_state64) / sizeof (uint32_t)))

struct x86_avx512_state64
{
    int32_t             fpu_reserved[2];
    uint16_t            fpu_fcw;           /* x87 FPU control word */
    uint16_t            fpu_fsw;           /* x87 FPU status word */
    uint8_t             fpu_ftw;           /* x87 FPU tag word */
    uint8_t             fpu_rsrv1;         /* reserved */ 
    uint16_t            fpu_fop;           /* x87 FPU Opcode */

    /* x87 FPU Instruction Pointer */
    uint32_t            fpu_ip;            /* offset */
    uint16_t            fpu_cs;            /* Selector */

    uint16_t            fpu_rsrv2;         /* reserved */

    /* x87 FPU Instruction Operand(Data) Pointer */
    uint32_t            fpu_dp;            /* offset */
    uint16_t            fpu_ds;            /* Selector */

    uint16_t            fpu_rsrv3;         /* reserved */
    uint32_t            fpu_mxcsr;         /* MXCSR Register state */
    uint32_t            fpu_mxcsrmask;     /* MXCSR mask */
    struct x86_mmst_reg fpu_stmm0;         /* ST0/MM0   */
    struct x86_mmst_reg fpu_stmm1;         /* ST1/MM1  */
    struct x86_mmst_reg fpu_stmm2;         /* ST2/MM2  */
    struct x86_mmst_reg fpu_stmm3;         /* ST3/MM3  */
    struct x86_mmst_reg fpu_stmm4;         /* ST4/MM4  */
    struct x86_mmst_reg fpu_stmm5;         /* ST5/MM5  */
    struct x86_mmst_reg fpu_stmm6;         /* ST6/MM6  */
    struct x86_mmst_reg fpu_stmm7;         /* ST7/MM7  */
    uint8_t             fpu_xmm0[16];      /* XMM 0  */
    uint8_t             fpu_xmm1[16];      /* XMM 1  */
    uint8_t             fpu_xmm2[16];      /* XMM 2  */
    uint8_t             fpu_xmm3[16];      /* XMM 3  */
    uint8_t             fpu_xmm4[16];      /* XMM 4  */
    uint8_t             fpu_xmm5[16];      /* XMM 5  */
    uint8_t             fpu_xmm6[16];      /* XMM 6  */
    uint8_t             fpu_xmm7[16];      /* XMM 7  */
    uint8_t             fpu_xmm8[16];      /* XMM 8  */
    uint8_t             fpu_xmm9[16];      /* XMM 9  */
    uint8_t             fpu_xmm10[16];     /* XMM 10  */
    uint8_t             fpu_xmm11[16];     /* XMM 11 */
    uint8_t             fpu_xmm12[16];     /* XMM 12  */
    uint8_t             fpu_xmm13[16];     /* XMM 13  */
    uint8_t             fpu_xmm14[16];     /* XMM 14  */
    uint8_t             fpu_xmm15[16];     /* XMM 15  */
    int8_t              fpu_rsrv4[6*16];   /* reserved */
    int32_t             fpu_reserved1;
    int8_t              avx_reserved1[64];
    uint8_t             fpu_ymmh0[16];     /* YMMH 0  */
    uint8_t             fpu_ymmh1[16];     /* YMMH 1  */
    uint8_t             fpu_ymmh2[16];     /* YMMH 2  */
    uint8_t             fpu_ymmh3[16];     /* YMMH 3  */
    uint8_t             fpu_ymmh4[16];     /* YMMH 4  */
    uint8_t             fpu_ymmh5[16];     /* YMMH 5  */
    uint8_t             fpu_ymmh6[16];     /* YMMH 6  */
    uint8_t             fpu_ymmh7[16];     /* YMMH 7  */
    uint8_t             fpu_ymmh8[16];     /* YMMH 8  */
    uint8_t             fpu_ymmh9[16];     /* YMMH 9  */
    uint8_t             fpu_ymmh10[16];    /* YMMH 10  */
    uint8_t             fpu_ymmh11[16];    /* YMMH 11  */
    uint8_t             fpu_ymmh12[16];    /* YMMH 12  */
    uint8_t             fpu_ymmh13[16];    /* YMMH 13  */
    uint8_t             fpu_ymmh14[16];    /* YMMH 14  */
    uint8_t             fpu_ymmh15[16];    /* YMMH 15  */
    uint8_t             fpu_k0[8];         /* K0 */
    uint8_t             fpu_k1[8];         /* K1 */
    uint8_t             fpu_k2[8];         /* K2 */
    uint8_t             fpu_k3[8];         /* K3 */
    uint8_t             fpu_k4[8];         /* K4 */
    uint8_t             fpu_k5[8];         /* K5 */
    uint8_t             fpu_k6[8];         /* K6 */
    uint8_t             fpu_k7[8];         /* K7 */
    uint8_t             fpu_zmmh0[32];     /* ZMMH 0  */
    uint8_t             fpu_zmmh1[32];     /* ZMMH 1  */
    uint8_t             fpu_zmmh2[32];     /* ZMMH 2  */
    uint8_t             fpu_zmmh3[32];     /* ZMMH 3  */
    uint8_t             fpu_zmmh4[32];     /* ZMMH 4  */
    uint8_t             fpu_zmmh5[32];     /* ZMMH 5  */
    uint8_t             fpu_zmmh6[32];     /* ZMMH 6  */
    uint8_t             fpu_zmmh7[32];     /* ZMMH 7  */
    uint8_t             fpu_zmmh8[32];     /* ZMMH 8  */
    uint8_t             fpu_zmmh9[32];     /* ZMMH 9  */
    uint8_t             fpu_zmmh10[32];    /* ZMMH 10  */
    uint8_t             fpu_zmmh11[32];    /* ZMMH 11  */
    uint8_t             fpu_zmmh12[32];    /* ZMMH 12  */
    uint8_t             fpu_zmmh13[32];    /* ZMMH 13  */
    uint8_t             fpu_zmmh14[32];    /* ZMMH 14  */
    uint8_t             fpu_zmmh15[32];    /* ZMMH 15  */
    uint8_t             fpu_zmm16[64];     /* ZMM 16  */
    uint8_t             fpu_zmm17[64];     /* ZMM 17  */
    uint8_t             fpu_zmm18[64];     /* ZMM 18  */
    uint8_t             fpu_zmm19[64];     /* ZMM 19  */
    uint8_t             fpu_zmm20[64];     /* ZMM 20  */
    uint8_t             fpu_zmm21[64];     /* ZMM 21  */
    uint8_t             fpu_zmm22[64];     /* ZMM 22  */
    uint8_t             fpu_zmm23[64];     /* ZMM 23  */
    uint8_t             fpu_zmm24[64];     /* ZMM 24  */
    uint8_t             fpu_zmm25[64];     /* ZMM 25  */
    uint8_t             fpu_zmm26[64];     /* ZMM 26  */
    uint8_t             fpu_zmm27[64];     /* ZMM 27  */
    uint8_t             fpu_zmm28[64];     /* ZMM 28  */
    uint8_t             fpu_zmm29[64];     /* ZMM 29  */
    uint8_t             fpu_zmm30[64];     /* ZMM 30  */
    uint8_t             fpu_zmm31[64];     /* ZMM 31  */
};
typedef struct x86_avx512_state64 x86_avx512_state64_t;
#define x86_AVX512_STATE64_COUNT ((uint32_t) (sizeof (struct x86_avx512_state64) / sizeof (uint32_t)))

struct x86_exception_state64
{
    uint16_t trapno;
    uint16_t cpu;
    uint32_t err;
    uint64_t faultvaddr;
};
typedef struct x86_exception_state64 x86_exception_state64_t;
#define x86_EXCEPTION_STATE64_COUNT ((uint32_t) (sizeof (struct x86_exception_state64) / sizeof (uint32_t)))

struct x86_debug_state64
{
    uint64_t dr0;
    uint64_t dr1;
    uint64_t dr2;
    uint64_t dr3;
    uint64_t dr4;
    uint64_t dr5;
    uint64_t dr6;
    uint64_t dr7;
};
typedef struct x86_debug_state64 x86_debug_state64_t;
#define x86_DEBUG_STATE64_COUNT ((uint32_t) (sizeof (struct x86_debug_state64) / sizeof (uint32_t)))

struct x86_cpmu_state64
{
    uint64_t ctrs[16];
};
typedef struct x86_cpmu_state64 x86_cpmu_state64_t;

struct x86_last_branch_record
{
    uint64_t from_ip;
    uint64_t to_ip;
    uint32_t mispredict;
    // uint32_t mispredict : 1,
    //     tsx_abort  : 1,
    //     in_tsx     : 1,
    //     cycle_count: 16,
    //     reserved   : 13;
};
typedef struct x86_last_branch_record x86_last_branch_record_t;

struct x86_last_branch_state
{
    int32_t                       lbr_count;
    uint32_t                      lbr_supported_tsx;
    // uint32_t                   lbr_supported_tsx : 1,
    //                            lbr_supported_cycle_count : 1,
    //                            reserved : 30;
#   define x86_LASTBRANCH_MAX  32
    struct x86_last_branch_record lbrs[x86_LASTBRANCH_MAX];
};
typedef struct x86_last_branch_state x86_last_branch_state_t;
#define x86_LAST_BRANCH_STATE_COUNT ((uint32_t) (sizeof (struct x86_last_branch_record) / sizeof (uint32_t)))

struct x86_pagein_state
{
    int32_t pagein_error;
};
typedef struct x86_pagein_state x86_pagein_state_t;
#define x86_PAGEIN_STATE_COUNT ((uint32_t) (sizeof (struct x86_pagein_state) / sizeof (uint32_t)))

/*
 * The format in which thread state is saved by Mach on this machine.  This
 * state flavor is most efficient for exception RPC's to kernel-loaded
 * servers, because copying can be avoided:
 */
struct x86_saved_state32 {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t cr2;    /* kernel esp stored by pusha - we save cr2 here later */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint16_t trapno;
    uint16_t cpu;
    uint32_t err;
    uint32_t eip;
    uint32_t cs;
    uint32_t efl;
    uint32_t uesp;
    uint32_t ss;
};
typedef struct x86_saved_state32 x86_saved_state32_t;
#define x86_SAVED_STATE32_COUNT ((uint32_t)(sizeof (struct x86_saved_state32)/sizeof(uint32_t)))

/*
 * This is the state pushed onto the 64-bit interrupt stack
 * on any exception/trap/interrupt.
 */
struct x86_64_intr_stack_frame {
	uint16_t        trapno;
	uint16_t        cpu;
	uint32_t        _pad;
	uint64_t        trapfn;
	uint64_t        err;
	uint64_t        rip;
	uint64_t        cs;
	uint64_t        rflags;
	uint64_t        rsp;
	uint64_t        ss;
};
typedef struct x86_64_intr_stack_frame x86_64_intr_stack_frame_t;

/*
 * thread state format for task running in 64bit long mode
 * in long mode, the same hardware frame is always pushed regardless
 * of whether there was a change in privilege level... therefore, there
 * is no need for an x86_saved_state64_from_kernel variant
 */
struct x86_saved_state64 {
    uint64_t                       rdi; /* arg0 for system call */
    uint64_t                       rsi;
    uint64_t                       rdx;
    uint64_t                       r10; /* R10 := RCX prior to syscall trap */
    uint64_t                       r8;
    uint64_t                       r9;  /* arg5 for system call */
    uint64_t                       cr2;
    uint64_t                       r15;
    uint64_t                       r14;
    uint64_t                       r13;
    uint64_t                       r12;
    uint64_t                       r11;
    uint64_t                       rbp;
    uint64_t                       rbx;
    uint64_t                       rcx;
    uint64_t                       rax;
    uint32_t                       gs;
    uint32_t                       fs;
    uint32_t                       ds;
    uint32_t                       es;
    struct x86_64_intr_stack_frame isf;
};
typedef struct x86_saved_state64 x86_saved_state64_t;
#define x86_SAVED_STATE64_COUNT ((uint32_t)(sizeof (struct x86_saved_state64)/sizeof(uint32_t)))

/*
 * ARM Thread Flavors
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/mach/arm/thread_status.h#L52-L157
 */
#define ARM_THREAD_STATE         1
#define ARM_UNIFIED_THREAD_STATE ARM_THREAD_STATE
#define ARM_VFP_STATE            2
#define ARM_EXCEPTION_STATE      3
#define ARM_DEBUG_STATE          4 /* pre-armv8 */
#define ARM_THREAD_STATE_NONE    5
#define ARM_THREAD_STATE64       6
#define ARM_EXCEPTION_STATE64    7
#define ARM_THREAD_STATE_LAST    8 /* legacy */
#define ARM_THREAD_STATE32       9
#define ARM_EXCEPTION_STATE64_V2 10

/* API */
#define ARM_DEBUG_STATE32        14
#define ARM_DEBUG_STATE64        15
#define ARM_NEON_STATE           16
#define ARM_NEON_STATE64         17
#define ARM_CPMU_STATE64         18

/* For kernel use */
#define ARM_SAVED_STATE32        20
#define ARM_SAVED_STATE64        21
#define ARM_NEON_SAVED_STATE32   22
#define ARM_NEON_SAVED_STATE64   23

#define ARM_PAGEIN_STATE         27

/* API */
#define ARM_SME_STATE            28
#define ARM_SVE_Z_STATE1         29
#define ARM_SVE_Z_STATE2         30
#define ARM_SVE_P_STATE          31
#define ARM_SME_ZA_STATE1        32
#define ARM_SME_ZA_STATE2        33
#define ARM_SME_ZA_STATE3        34
#define ARM_SME_ZA_STATE4        35
#define ARM_SME_ZA_STATE5        36
#define ARM_SME_ZA_STATE6        37
#define ARM_SME_ZA_STATE7        38
#define ARM_SME_ZA_STATE8        39
#define ARM_SME_ZA_STATE9        40
#define ARM_SME_ZA_STATE10       41
#define ARM_SME_ZA_STATE11       42
#define ARM_SME_ZA_STATE12       43
#define ARM_SME_ZA_STATE13       44
#define ARM_SME_ZA_STATE14       45
#define ARM_SME_ZA_STATE15       46
#define ARM_SME_ZA_STATE16       47
#define ARM_SME2_STATE           48
#define ARM_SME_SAVED_STATE      49
#define ARM_THREAD_STATE_FLAVORS 50 /* This must be updated to 1 more than the highest numerical state flavor */

#ifndef ARM_STATE_FLAVOR_IS_OTHER_VALID
#define ARM_STATE_FLAVOR_IS_OTHER_VALID(_flavor_) 0
#endif

#define ARM_FLAVOR_MODIFIES_CORE_CPU_REGISTERS(x) \
((x == ARM_THREAD_STATE)   ||   \
 (x == ARM_THREAD_STATE32) ||   \
 (x == ARM_THREAD_STATE64))

#define ARM_VALID_THREAD_STATE_FLAVOR(x)  \
    ((x == ARM_THREAD_STATE) ||           \
     (x == ARM_VFP_STATE) ||              \
     (x == ARM_EXCEPTION_STATE) ||        \
     (x == ARM_DEBUG_STATE) ||            \
     (x == ARM_THREAD_STATE_NONE) ||      \
     (x == ARM_THREAD_STATE32) ||         \
     (x == ARM_THREAD_STATE64) ||         \
     (x == ARM_EXCEPTION_STATE64) ||      \
     (x == ARM_EXCEPTION_STATE64_V2) ||   \
     (x == ARM_NEON_STATE) ||             \
     (x == ARM_NEON_STATE64) ||           \
     (x == ARM_DEBUG_STATE32) ||          \
     (x == ARM_DEBUG_STATE64) ||          \
     (x == ARM_PAGEIN_STATE) ||           \
     (ARM_STATE_FLAVOR_IS_OTHER_VALID(x)))

// Reference:
// https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/mach/arm/_structs.h#L87-L107
struct arm_exception_state32
{
    uint32_t exception;   /* number of arm exception taken */
    uint32_t fsr;         /* Fault status */
    uint32_t far;         /* Virtual Fault Address */
};
#define ARM_EXCEPTION_STATE32_COUNT ((uint32_t)(sizeof(struct arm_exception_state32)/sizeof(uint32_t)))

struct arm_exception_state64
{
    uint64_t far;         /* Virtual Fault Address */
    uint64_t esr;         /* Exception syndrome */
    uint32_t exception;   /* number of arm exception taken */
};
#define ARM_EXCEPTION_STATE64_COUNT ((uint32_t)(sizeof(struct arm_exception_state64)/sizeof(uint32_t)))

struct arm_exception_state64_v2
{
    uint64_t far;         /* Virtual Fault Address */
    uint64_t esr;         /* Exception syndrome */
};
#define ARM_EXCEPTION_STATE64_V2_COUNT ((uint32_t)(sizeof(struct arm_exception_state64_v2)/sizeof(uint32_t)))

struct arm_thread_state32
{
    uint32_t r[13]; /* General purpose register r0-r12 */
    uint32_t sp;    /* Stack pointer r13 */
    uint32_t lr;    /* Link register r14 */
    uint32_t pc;    /* Program counter r15 */
    uint32_t cpsr;  /* Current program status register */
};
#define ARM_THREAD_STATE32_COUNT ((uint32_t)(sizeof(struct arm_thread_state32)/sizeof(uint32_t)))

struct arm_thread_state64
{
    uint64_t x[29]; /* General purpose registers x0-x28 */
    uint64_t fp;    /* Frame pointer x29 */
    uint64_t lr;    /* Link register x30 */
    uint64_t sp;    /* Stack pointer x31 */
    uint64_t pc;    /* Program counter */
    uint32_t cpsr;  /* Current program status register */
    uint32_t flags; /* Flags describing structure format */
};
#define ARM_THREAD_STATE64_COUNT ((uint32_t)(sizeof(struct arm_thread_state64)/sizeof(uint32_t)))

struct arm_vfp_state
{
    uint32_t r[64];
    uint32_t fpscr;
};
#define ARM_VFP_STATE_COUNT ((uint32_t)(sizeof(struct arm_vfp_state)/sizeof(uint32_t)))

struct arm_neon_state64
{
    uint128_t  v[32];
    uint32_t   fpsr;
    uint32_t   fpcr;
};
#define ARM_NEON_STATE64_COUNT ((uint32_t)(sizeof(struct arm_neon_state64)/sizeof(uint32_t)))

struct arm_pagein_state
{
    int32_t pagein_error;
};
#define ARM_PAGEIN_STATE_COUNT ((uint32_t)(sizeof(struct arm_pagein_state)/sizeof(uint32_t)))

struct arm_sme_state
{
    uint64_t svcr;
    uint64_t tpidr2_el0;
    uint16_t svl_b;
};
#define ARM_SME_STATE_COUNT ((uint32_t)(sizeof(struct arm_sme_state)/sizeof(uint32_t)))

struct arm_sve_z_state
{
    uint8_t z[16][256];
} CC_align(4);
#define ARM_SVE_Z_STATE_COUNT ((uint32_t)(sizeof(struct arm_sve_z_state)/sizeof(uint32_t)))

struct arm_sve_p_state
{
    uint8_t p[16][256 / 8];
} CC_align(4);
#define ARM_SVE_P_STATE_COUNT ((uint32_t)(sizeof(struct arm_sve_p_state)/sizeof(uint32_t)))

struct arm_sme_za_state
{
    uint8_t za[4096];
} CC_align(4);
#define ARM_SME_ZA_STATE_COUNT ((uint32_t)(sizeof(struct arm_sme_za_state)/sizeof(uint32_t)))

struct arm_sme2_state
{
    uint8_t zt0[64];
} CC_align(4);
#define ARM_SME2_STATE_COUNT ((uint32_t)(sizeof(struct arm_sme2_state)/sizeof(uint32_t)))

struct arm_legacy_debug_state
{
    uint32_t bvr[16];
    uint32_t bcr[16];
    uint32_t wvr[16];
    uint32_t wcr[16];
};
#define ARM_LEGACY_DEBUG_STATE_COUNT ((uint32_t)(sizeof(struct arm_legacy_debug_state)/sizeof(uint32_t)))

struct arm_debug_state32
{
    uint32_t bvr[16];
    uint32_t bcr[16];
    uint32_t wvr[16];
    uint32_t wcr[16];
    uint64_t mdscr_el1; /* Bit 0 is SS (Hardware Single Step) */
};
#define ARM_DEBUG_STATE32_COUNT ((uint32_t)(sizeof(struct arm_debug_state32)/sizeof(uint32_t)))

struct arm_debug_state64
{
    uint64_t bvr[16];
    uint64_t bcr[16];
    uint64_t wvr[16];
    uint64_t wcr[16];
    uint64_t mdscr_el1; /* Bit 0 is SS (Hardware Single Step) */
};
#define ARM_DEBUG_STATE64_COUNT ((uint32_t)(sizeof(struct arm_debug_state64)/sizeof(uint32_t)))

struct arm_cpmu_state64
{
    uint64_t ctrs[16];
};
#define ARM_CPMU_STATE64_COUNT ((uint32_t)(sizeof(struct arm_cpmu_state64)/sizeof(uint32_t)))

struct arm_sme_context {
    uint8_t zt0[64];
    uint8_t z_p_za[];
};

struct arm_sme_saved_state {
	uint32_t               flavor;
	uint32_t               count;
	uint64_t               svcr;
	uint16_t               svl_b;
	struct arm_sme_context context;
};
#define ARM_SME_Z_SIZE(svl_b) (((size_t)svl_b)<<5)
#define ARM_SME_P_SIZE(svl_b) (((size_t)svl_b)<<1)
#define ARM_SME_ZA_SIZE(svl_b) (((size_t)svl_b)*((size_t)svl_b))
#define ARM_SME_SAVED_STATE_COUNT(svl_b) ((uint32_t)(\
    (sizeof(struct arm_sme_saved_state) \
    + ARM_SME_Z_SIZE(svl_b) \
    + ARM_SME_P_SIZE(svl_b) \
    + ARM_SME_ZA_SIZE(svl_b)) / sizeof(uint32_t)))
static inline size_t arm_sme_z_size(uint16_t svl_b)
{
	return ((size_t)svl_b) << 5;
}
static inline size_t arm_sme_p_size(uint16_t svl_b)
{
	return ((size_t)svl_b) << 1;
}
static inline size_t arm_sme_za_size(uint16_t svl_b)
{
	return ((size_t)svl_b) * ((size_t)svl_b);
}
static inline uint32_t arm_sme_saved_state_count(uint16_t svl_b)
{
	// assert(svl_b % 16 == 0);
	size_t size = sizeof(struct arm_sme_saved_state) +
	    arm_sme_z_size(svl_b) +
	    arm_sme_p_size(svl_b) +
	    arm_sme_za_size(svl_b);
	return (uint32_t)(size / sizeof(uint32_t));
}

/* Combine all thread states */
union thread_state {
    union {
        struct arm_thread_state32 thread_32;
        struct arm_thread_state64 thread_64;
        struct arm_vfp_state vfp;
        struct arm_exception_state32 exception_32;
        struct arm_exception_state64 exception_64;
        struct arm_exception_state64_v2 exception_v2_64;
        struct arm_legacy_debug_state debug_legacy;
        struct arm_debug_state32 debug_32;
        struct arm_debug_state64 debug_64;
        struct arm_neon_state64 neon_64;
        struct arm_pagein_state pagein;
        struct arm_sme_state sme;
        struct arm_sme2_state sme2;
        struct arm_sve_z_state sve_z;
        struct arm_sve_p_state sve_p;
        struct arm_sme_za_state sme_za;
        struct arm_cpmu_state64 cpmu_64;
    } arm;
    union {
        struct x86_thread_state32 thread_32;
        struct x86_thread_state64 thread_64;
        struct x86_thread_full_state64 thread_full64;
        struct x86_float_state32 float_32;
        struct x86_float_state64 float_64;
        struct x86_avx_state32 avx_32;
        struct x86_avx_state64 avx_64;
        struct x86_avx512_state32 avx512_32;
        struct x86_avx512_state64 avx512_64;
        struct x86_exception_state32 exception_32;
        struct x86_exception_state64 exception_64;
        struct x86_debug_state32 debug_32;
        struct x86_debug_state64 debug_64;
        struct x86_cpmu_state64 cpmu_64;
        struct x86_last_branch_state last_branch;
        struct x86_pagein_state pagein;
        struct x86_saved_state32 saved_32;
        struct x86_saved_state64 saved_64;
    } x86;
};

/*
 * Magic numbers used by Code Signing
 * Reference:
 * https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/osfmk/kern/cs_blobs.h#L91-L171
 */
enum {
	CSMAGIC_REQUIREMENT = 0xfade0c00,               /* single Requirement blob */
	CSMAGIC_REQUIREMENTS = 0xfade0c01,              /* Requirements vector (internal requirements) */
	CSMAGIC_CODEDIRECTORY = 0xfade0c02,             /* CodeDirectory blob */
	CSMAGIC_EMBEDDED_SIGNATURE = 0xfade0cc0, /* embedded form of signature data */
	CSMAGIC_EMBEDDED_SIGNATURE_OLD = 0xfade0b02,    /* XXX */
	CSMAGIC_EMBEDDED_ENTITLEMENTS = 0xfade7171,     /* embedded entitlements */
	CSMAGIC_EMBEDDED_DER_ENTITLEMENTS = 0xfade7172, /* embedded DER encoded entitlements */
	CSMAGIC_DETACHED_SIGNATURE = 0xfade0cc1, /* multi-arch collection of embedded signatures */
	CSMAGIC_BLOBWRAPPER = 0xfade0b01,       /* CMS Signature, among other things */
	CSMAGIC_EMBEDDED_LAUNCH_CONSTRAINT = 0xfade8181, /* Light weight code requirement */

	CS_SUPPORTSSCATTER = 0x20100,
	CS_SUPPORTSTEAMID = 0x20200,
	CS_SUPPORTSCODELIMIT64 = 0x20300,
	CS_SUPPORTSEXECSEG = 0x20400,
	CS_SUPPORTSRUNTIME = 0x20500,
	CS_SUPPORTSLINKAGE = 0x20600,

	CSSLOT_CODEDIRECTORY = 0,                               /* slot index for CodeDirectory */
	CSSLOT_INFOSLOT = 1,
	CSSLOT_REQUIREMENTS = 2,
	CSSLOT_RESOURCEDIR = 3,
	CSSLOT_APPLICATION = 4,
	CSSLOT_ENTITLEMENTS = 5,
	CSSLOT_DER_ENTITLEMENTS = 7,
	CSSLOT_LAUNCH_CONSTRAINT_SELF = 8,
	CSSLOT_LAUNCH_CONSTRAINT_PARENT = 9,
	CSSLOT_LAUNCH_CONSTRAINT_RESPONSIBLE = 10,
	CSSLOT_LIBRARY_CONSTRAINT = 11,

	CSSLOT_ALTERNATE_CODEDIRECTORIES = 0x1000, /* first alternate CodeDirectory, if any */
	CSSLOT_ALTERNATE_CODEDIRECTORY_MAX = 5,         /* max number of alternate CD slots */
	CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT = CSSLOT_ALTERNATE_CODEDIRECTORIES + CSSLOT_ALTERNATE_CODEDIRECTORY_MAX, /* one past the last */

	CSSLOT_SIGNATURESLOT = 0x10000,                 /* CMS Signature */
	CSSLOT_IDENTIFICATIONSLOT = 0x10001,
	CSSLOT_TICKETSLOT = 0x10002,

	CSTYPE_INDEX_REQUIREMENTS = 0x00000002,         /* compat with amfi */
	CSTYPE_INDEX_ENTITLEMENTS = 0x00000005,         /* compat with amfi */

	CS_HASHTYPE_SHA1 = 1,
	CS_HASHTYPE_SHA256 = 2,
	CS_HASHTYPE_SHA256_TRUNCATED = 3,
	CS_HASHTYPE_SHA384 = 4,

	CS_SHA1_LEN = 20,
	CS_SHA256_LEN = 32,
	CS_SHA256_TRUNCATED_LEN = 20,

	CS_CDHASH_LEN = 20,                                             /* always - larger hashes are truncated */
	CS_HASH_MAX_SIZE = 48, /* max size of the hash we'll support */

    /*
     * Currently only to support Legacy VPN plugins, and Mac App Store
     * but intended to replace all the various platform code, dev code etc. bits.
     */
	CS_SIGNER_TYPE_UNKNOWN = 0,
	CS_SIGNER_TYPE_LEGACYVPN = 5,
	CS_SIGNER_TYPE_MAC_APP_STORE = 6,

	CS_SUPPL_SIGNER_TYPE_UNKNOWN = 0,
	CS_SUPPL_SIGNER_TYPE_TRUSTCACHE = 7,
	CS_SUPPL_SIGNER_TYPE_LOCAL = 8,

	CS_SIGNER_TYPE_OOPJIT = 9,

	/* Validation categories used for trusted launch environment */
	CS_VALIDATION_CATEGORY_INVALID = 0,
	CS_VALIDATION_CATEGORY_PLATFORM = 1,
	CS_VALIDATION_CATEGORY_TESTFLIGHT = 2,
	CS_VALIDATION_CATEGORY_DEVELOPMENT = 3,
	CS_VALIDATION_CATEGORY_APP_STORE = 4,
	CS_VALIDATION_CATEGORY_ENTERPRISE = 5,
	CS_VALIDATION_CATEGORY_DEVELOPER_ID = 6,
	CS_VALIDATION_CATEGORY_LOCAL_SIGNING = 7,
	CS_VALIDATION_CATEGORY_ROSETTA = 8,
	CS_VALIDATION_CATEGORY_OOPJIT = 9,
	CS_VALIDATION_CATEGORY_NONE = 10,
};
