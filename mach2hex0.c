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
 * - https://github.com/aidansteele/osx-abi-macho-file-format-reference
 * - https://www.macsyscalls.com/en/syscall
 *
 * Hex0 Format:
 * - https://bootstrapping.miraheze.org/wiki/Hex0
 * 
 * Oficial Git Repo: https://github.com/Lohann/stage0-macos
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <inttypes.h>
#include "mach2hex0.h"

/******************************/
/* Detect C Compiler Features */
/******************************/

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

/* Detect C Compiler 'variadic arguments' support */
#if CC_include(<stdbool.h>) && defined(__jose__)
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

/* Helpers for conversion between little-endian <=> big-endian
 * OBS: Assumes mach-o components are encoded in little-endian */
#if defined(_MSC_VER) \
    || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ \
    || (defined(__LITTLE_ENDIAN__) && __LITTLE_ENDIAN__)
#   define to_u16be(val) CC_bswap16(val)
#   define to_u32be(val) CC_bswap32(val)
#   define to_u64be(val) CC_bswap64(val)
#   define to_u16le(val) ((uint16_t)(val))
#   define to_u32le(val) ((uint32_t)(val))
#   define to_u64le(val) ((uint64_t)(val))
#else
#   define to_u16be(val) ((uint16_t)(val))
#   define to_u32be(val) ((uint32_t)(val))
#   define to_u64be(val) ((uint64_t)(val))
#   define to_u16le(val) CC_bswap16(val)
#   define to_u32le(val) CC_bswap32(val)
#   define to_u64le(val) CC_bswap64(val)
#endif

/* In ANSI-C (C99) integer overflow behavior is undefined,
 * following helpers enforces consistent behavior. */
static inline uint32_t CC_shr_u32(uint32_t lhs, uint8_t rhs) { return lhs >> rhs; }
static inline uint32_t CC_wrap_u32(uint32_t val, uint8_t bits) { return val & CC_shr_u32(UINT32_MAX, 32 - bits); }
static inline bool CC_add_u32(uint32_t *res, uint32_t lhs, uint32_t rhs, uint8_t bits) {
#if CC_has_builtin(add_overflow) || defined(__GNUC__)
    uint32_t full_res;
    bool overflow = __builtin_add_overflow(lhs, rhs, &full_res);
    *res = CC_wrap_u32(full_res, bits);
    return overflow || full_res < UINT32_C(0) || full_res > UINT32_MAX;
#else
    *res = CC_wrap_u32(lhs, rhs, bits);
    return *res < lhs;
#endif
}

/* Helper for convert macro definitions to cstring */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/************************/
/* structs and typedefs */
/************************/

/* Read-only pointer to raw bytes */
typedef const uint8_t *bytes_t;

/* Name and optional description of a Mach-O constant */
typedef struct const_info_t {
    uint32_t value;
    const char *name; /* Non-NULL */
    const char *desc; /* Nullable */
} const_info_t;

/* Mach-O file decoded */
#define NCMDS_MAX 255
typedef struct mach_decoded_t
{
    struct mach_header_64 header;
    const struct load_command *load_commands[NCMDS_MAX + 1]; /* NULL terminated */
    const uint32_t *src;
    size_t len;
} mach_decoded_t;

/**********************/
/** Mach-O Constants **/
/**********************/
/* Helper macro for build `const_info_t`. */
#define entry(n,d) ((const_info_t){ .value = to_u32le(n), .name = #n, .desc = d })

/* Mach-O MAGIC (first 4 bytes of any mach-o file) */
static const_info_t mach_magics_s[4] = {
    entry(MH_MAGIC,                    "The mach magic number (32-bit little-endian)"),
    entry(MH_MAGIC_64,                 "The mach magic number (64-bit little-endian)"),
    entry(MH_CIGAM,                    "The mach magic number (32-bit big-endian)"),
    entry(MH_CIGAM_64,                 "The mach magic number (64-bit big-endian)")
};

/* CPU TYPES `mach_header->cputype` */
static const_info_t cputypes_s[16] = {
    entry(CPU_TYPE_ANY,       NULL), /* 0xFFFFFFFF */
    entry(CPU_TYPE_VAX,       NULL), /* 0x00000001 */
    entry(CPU_TYPE_MC680x0,   NULL), /* 0x00000006 */
    entry(CPU_TYPE_X86,       NULL), /* 0x00000007 */
    entry(CPU_TYPE_I386,      NULL), /* 0x00000007 (alias to x86) */
    entry(CPU_TYPE_X86_64,    NULL), /* 0x01000007 */
    entry(CPU_TYPE_MC98000,   NULL), /* 0x0000000A */
    entry(CPU_TYPE_HPPA,      NULL), /* 0x0000000B */
    entry(CPU_TYPE_ARM,       NULL), /* 0x0000000C */
    entry(CPU_TYPE_ARM64,     NULL), /* 0x0100000C */
    entry(CPU_TYPE_ARM64_32,  NULL), /* 0x0200000C */
    entry(CPU_TYPE_MC88000,   NULL), /* 0x0000000D */
    entry(CPU_TYPE_SPARC,     NULL), /* 0x0000000E */
    entry(CPU_TYPE_I860,      NULL), /* 0x0000000F */
    entry(CPU_TYPE_POWERPC,   NULL), /* 0x00000012 */
    entry(CPU_TYPE_POWERPC64, NULL)  /* 0x01000012 */
};

/* VAX CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_vax_s[13] = {
    entry(CPU_SUBTYPE_VAX_ALL, NULL), /* 0x00 */
    entry(CPU_SUBTYPE_VAX780,  NULL), /* 0x01 */
    entry(CPU_SUBTYPE_VAX785,  NULL), /* 0x02 */
    entry(CPU_SUBTYPE_VAX750,  NULL), /* 0x03 */
    entry(CPU_SUBTYPE_VAX730,  NULL), /* 0x04 */
    entry(CPU_SUBTYPE_UVAXI,   NULL), /* 0x05 */
    entry(CPU_SUBTYPE_UVAXII,  NULL), /* 0x06 */
    entry(CPU_SUBTYPE_VAX8200, NULL), /* 0x07 */
    entry(CPU_SUBTYPE_VAX8500, NULL), /* 0x08 */
    entry(CPU_SUBTYPE_VAX8600, NULL), /* 0x09 */
    entry(CPU_SUBTYPE_VAX8650, NULL), /* 0x0A */
    entry(CPU_SUBTYPE_VAX8800, NULL), /* 0x0B */
    entry(CPU_SUBTYPE_UVAXIII, NULL), /* 0x0C */
};
/* MC680x0 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_mc680x0_s[4] = {
    entry(CPU_SUBTYPE_MC68030,      NULL),     /* 0x01 */
    entry(CPU_SUBTYPE_MC68040,      "compat"), /* 0x02 */
    entry(CPU_SUBTYPE_MC68030_ONLY, NULL),     /* 0x03 */
    entry(CPU_SUBTYPE_MC680x0_ALL,  NULL)      /* 0x01 */
};
/* I386 (x86) CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_x86_s[22] = {
    entry(CPU_SUBTYPE_XEON_MP,         NULL),
    entry(CPU_SUBTYPE_XEON,            NULL),
    entry(CPU_SUBTYPE_ITANIUM_2,       NULL),
    entry(CPU_SUBTYPE_ITANIUM,         NULL),
    entry(CPU_SUBTYPE_PENTIUM_4_M,     NULL),
    entry(CPU_SUBTYPE_PENTIUM_4,       NULL),
    entry(CPU_SUBTYPE_PENTIUM_M,       NULL),
    entry(CPU_SUBTYPE_PENTIUM_3_XEON,  NULL),
    entry(CPU_SUBTYPE_PENTIUM_3_M,     NULL),
    entry(CPU_SUBTYPE_PENTIUM_3,       NULL),
    entry(CPU_SUBTYPE_CELERON_MOBILE,  NULL),
    entry(CPU_SUBTYPE_CELERON,         NULL),
    entry(CPU_SUBTYPE_PENTII_M5,       NULL),
    entry(CPU_SUBTYPE_PENTII_M3,       NULL),
    entry(CPU_SUBTYPE_PENTPRO,         NULL),
    entry(CPU_SUBTYPE_PENT,            NULL),
    entry(CPU_SUBTYPE_586,             NULL),
    entry(CPU_SUBTYPE_486SX,           NULL),
    entry(CPU_SUBTYPE_486,             NULL),
    entry(CPU_SUBTYPE_386,             NULL),
    entry(CPU_SUBTYPE_I386_ALL,        NULL),
    entry(CPU_SUBTYPE_INTEL_MODEL_ALL, NULL)
};
/* X86_64 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_x86_64_s[3] = {
    entry(CPU_SUBTYPE_X86_64_H,   "Haswell feature subset"),
    entry(CPU_SUBTYPE_X86_ARCH1,  NULL),
    entry(CPU_SUBTYPE_X86_64_ALL, NULL)
    // ((const_info_t){
    //     .value = to_u32le(CPU_SUBTYPE_I386_ALL | CPU_SUBTYPE_LIB64),
    //     .name = "CPU_SUBTYPE_I386_ALL | CPU_SUBTYPE_LIB64",
    //     .desc = NULL
    // })
};
/* ARM CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_arm32_s[17] = {
    entry(CPU_SUBTYPE_ARM_V8_1M_MAIN, NULL),
    entry(CPU_SUBTYPE_ARM_V8M_BASE,   NULL),
    entry(CPU_SUBTYPE_ARM_V8M_MAIN,   NULL),
    entry(CPU_SUBTYPE_ARM_V8M,        NULL),
    entry(CPU_SUBTYPE_ARM_V7EM,       NULL),
    entry(CPU_SUBTYPE_ARM_V7M,        NULL),
    entry(CPU_SUBTYPE_ARM_V6M,        NULL),
    entry(CPU_SUBTYPE_ARM_V8,         NULL),
    entry(CPU_SUBTYPE_ARM_V7K,        NULL),
    entry(CPU_SUBTYPE_ARM_V7S,        NULL),
    entry(CPU_SUBTYPE_ARM_V7F,        NULL),
    entry(CPU_SUBTYPE_ARM_V7,         NULL),
    entry(CPU_SUBTYPE_ARM_XSCALE,     NULL),
    entry(CPU_SUBTYPE_ARM_V5TEJ,      NULL),
    entry(CPU_SUBTYPE_ARM_V6,         NULL),
    entry(CPU_SUBTYPE_ARM_V4T,        NULL),
    entry(CPU_SUBTYPE_ARM_ALL,        NULL)
};
/* ARM64 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_arm64_s[3] = {
    entry(CPU_SUBTYPE_ARM64E,    NULL),
    entry(CPU_SUBTYPE_ARM64_V8,  NULL),
    entry(CPU_SUBTYPE_ARM64_ALL, NULL)
};
/* ARM64_32 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_arm64_32_s[2] = {
    entry(CPU_SUBTYPE_ARM64_32_V8,  NULL),
    entry(CPU_SUBTYPE_ARM64_32_ALL, NULL)
};
/* MC98000 (PowerPC) CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_mc98000_s[2] = {
    entry(CPU_SUBTYPE_MC98601,   NULL),
    entry(CPU_SUBTYPE_MC98000_ALL,  NULL)
};
/* MC88000 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_mc88000_s[3] = {
    entry(CPU_SUBTYPE_MC88110,      NULL),
    entry(CPU_SUBTYPE_MC88100,      NULL),
    entry(CPU_SUBTYPE_MC88000_ALL,  NULL)
};
/* Mips CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_mips_s[8] = {
    entry(CPU_SUBTYPE_MIPS_R3000,  NULL),
    entry(CPU_SUBTYPE_MIPS_R3000a, "3max"),
    entry(CPU_SUBTYPE_MIPS_R2000,  NULL),
    entry(CPU_SUBTYPE_MIPS_R2000a, "pmax"),
    entry(CPU_SUBTYPE_MIPS_R2800,  NULL),
    entry(CPU_SUBTYPE_MIPS_R2600,  NULL),
    entry(CPU_SUBTYPE_MIPS_R2300,  NULL),
    entry(CPU_SUBTYPE_MIPS_ALL,    NULL)
};
/* HPPA CPU subtypes `mach_header->cpusubtype` */
static const_info_t cpusubtypes_hppa_s[3] = {
    entry(CPU_SUBTYPE_HPPA_7100LC,  NULL),
    entry(CPU_SUBTYPE_HPPA_7100,    "compat"),
    entry(CPU_SUBTYPE_HPPA_ALL,     NULL)
};
/* SPARC CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_sparc_s[1] = {
    entry(CPU_SUBTYPE_SPARC_ALL,     NULL)
};
/* I860 CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_i860_s[2] = {
    entry(CPU_SUBTYPE_I860_860, NULL),
    entry(CPU_SUBTYPE_I860_ALL, NULL)
};
/* PowerPC CPU SUBTYPES `mach_header->cpusubtype` */
static const_info_t cpusubtypes_powerpc_s[13] = {
    entry(CPU_SUBTYPE_POWERPC_970,   NULL),
    entry(CPU_SUBTYPE_POWERPC_7450,  NULL),
    entry(CPU_SUBTYPE_POWERPC_7400,  NULL),
    entry(CPU_SUBTYPE_POWERPC_750,   NULL),
    entry(CPU_SUBTYPE_POWERPC_620,   NULL),
    entry(CPU_SUBTYPE_POWERPC_604e,  NULL),
    entry(CPU_SUBTYPE_POWERPC_604,   NULL),
    entry(CPU_SUBTYPE_POWERPC_603ev, NULL),
    entry(CPU_SUBTYPE_POWERPC_603e,  NULL),
    entry(CPU_SUBTYPE_POWERPC_603,   NULL),
    entry(CPU_SUBTYPE_POWERPC_602,   NULL),
    entry(CPU_SUBTYPE_POWERPC_601,   NULL),
    entry(CPU_SUBTYPE_POWERPC_ALL,   NULL)
};

/* Mach-O FILE TYPE `mach_header->type` */
static const_info_t filetypes_s[11] = {
    entry(MH_OBJECT,      "relocatable object file"),
    entry(MH_EXECUTE,     "demand paged executable file"),
    entry(MH_FVMLIB,      "fixed VM shared library file"),
    entry(MH_CORE,        "core file"),
    entry(MH_PRELOAD,     "preloaded executable file"),
    entry(MH_DYLIB,       "dynamically bound shared library"),
    entry(MH_DYLINKER,    "dynamic link editor"),
    entry(MH_BUNDLE,      "dynamically bound bundle file"),
    entry(MH_DYLIB_STUB,  "shared library stub for static linking only, no section contents"),
    entry(MH_DSYM,        "companion file with only debug sections"),
    entry(MH_KEXT_BUNDLE, "x86_64 kexts")
};

/* Mach-o FILE FLAGS `mach_header->flags` */
static const_info_t headerflags_s[29] = {
    entry(MH_NOUNDEFS,                      "the object file has no undefined references when it "
                                            "was built"),
    entry(MH_INCRLINK,                      "the object file is the output of an incremental link "
                                            "against a base file and can't be link edited again"),
    entry(MH_DYLDLINK,                      "the object file is input for the dynamic linker and "
                                            "can't be statically link edited again"),
    entry(MH_BINDATLOAD,                    "the object file's undefined references are bound by "
                                            "the dynamic linker when loaded."),
    entry(MH_PREBOUND,                      "the file has its dynamic undefined references "
                                            "prebound."),
    entry(MH_SPLIT_SEGS,                    "the file has its read-only and read-write segments "
                                            "split"),
    entry(MH_LAZY_INIT,                     "the shared library init routine is to be run lazily "
                                            "via catching memory faults to its writeable segments "
                                            "(obsolete)"),
    entry(MH_TWOLEVEL,                      "the image is using two-level name space bindings"),
    entry(MH_FORCE_FLAT,                    "the executable is forcing all images to use flat "
                                            "name space bindings"),
    entry(MH_NOMULTIDEFS,                   "this umbrella guarantees no multiple definitions of "
                                            "symbols in its sub-images so the two-level namespace "
                                            "hints can always be used."),
    entry(MH_NOFIXPREBINDING,               "do not have dyld notify the prebinding agent about "
                                            "this executable"),
    entry(MH_PREBINDABLE,                   "the binary is not prebound but can have its "
                                            "prebinding redone. only used when MH_PREBOUND is not "
                                            "set."),
    entry(MH_ALLMODSBOUND,                  "indicates that this binary binds to all two-level "
                                            "namespace modules of its dependent libraries. only "
                                            "used when MH_PREBINDABLE and MH_TWOLEVEL are both "
                                            "set."),
    entry(MH_SUBSECTIONS_VIA_SYMBOLS,       "safe to divide up the sections into sub-sections via "
                                            "symbols for dead code stripping"),
    entry(MH_CANONICAL,                     "the binary has been canonicalized via the unprebind "
                                            "operation"),
    entry(MH_WEAK_DEFINES,                  "the final linked image contains external weak "
                                            "symbols"),
    entry(MH_BINDS_TO_WEAK,                 "the final linked image uses weak symbols"),
    entry(MH_ALLOW_STACK_EXECUTION,         "When this bit is set, all stacks in the task will be "
                                            "given stack execution privilege. Only used in "
                                            "MH_EXECUTE filetypes."),
    entry(MH_ROOT_SAFE,                     "When this bit is set, the binary declares it is safe "
                                            "for use in processes with uid zero"),
    entry(MH_SETUID_SAFE,                   "When this bit is set, the binary declares it is safe "
                                            "for use in processes when issetugid() is true"),
    entry(MH_NO_REEXPORTED_DYLIBS,          "When this bit is set on a dylib, the static linker "
                                            "does not need to examine dependent dylibs to see if "
                                            "any are re-exported"),
    entry(MH_PIE,                           "When this bit is set, the OS will load the main "
                                            "executable at a random address. Only used in "
                                            "MH_EXECUTE filetypes."),
    entry(MH_DEAD_STRIPPABLE_DYLIB,         "Only for use on dylibs.  When linking against a "
                                            "dylib that has this bit set, the static linker will "
                                            "automatically not create a LC_LOAD_DYLIB load "
                                            "command to the dylib if no symbols are being "
                                            "referenced from the dylib."),
    entry(MH_HAS_TLV_DESCRIPTORS,           "Contains a section of type S_THREAD_LOCAL_VARIABLES"),
    entry(MH_NO_HEAP_EXECUTION,             "When this bit is set, the OS will run the main "
                                            "executable with a non-executable heap even on "
                                            "platforms (e.g. x86) that don't require it. Only "
                                            "used in MH_EXECUTE filetypes."),
    entry(MH_APP_EXTENSION_SAFE,            "The code was linked for use in an application "
                                            "extension."),
    entry(MH_NLIST_OUTOFSYNC_WITH_DYLDINFO, "The external symbols listed in the nlist symbol "
                                            "table do not include all the symbols listed in "
                                            "the dyld info."),
    entry(MH_SIM_SUPPORT,                   "Allow LC_MIN_VERSION_MACOS and LC_BUILD_VERSION load "
                                            "commands with the platforms macOS, iOSMac, "
                                            "iOSSimulator, tvOSSimulator and watchOSSimulator."),
    entry(MH_DYLIB_IN_CACHE,                "Only for use on dylibs. When this bit is set, the "
                                            "dylib is part of the dyld shared cache, rather than "
                                            "loose in the filesystem.")
};

/* Mach-O LOAD COMMANDS */
static const_info_t load_commands_s[54] = {
    /* Constants for the cmd field of all load commands, the type */
    entry(LC_SEGMENT,                  "segment of this file to be mapped"),
    entry(LC_SYMTAB,                   "link-edit gdb symbol table info (obsolete)"),
    entry(LC_SYMSEG,                   "link-edit gdb symbol table info (obsolete)"),
    entry(LC_THREAD,                   "thread"),
    entry(LC_UNIXTHREAD,               "unix thread (includes a stack)"),
    entry(LC_LOADFVMLIB,               "load a specified fixed VM shared library"),
    entry(LC_IDFVMLIB,                 "fixed VM shared library identification"),
    entry(LC_IDENT,                    "object identification info (obsolete)"),
    entry(LC_FVMFILE,                  "fixed VM file inclusion (internal use)"),
    entry(LC_PREPAGE,                  "prepage command (internal use)"),
    entry(LC_DYSYMTAB,                 "dynamic link-edit symbol table info"),
    entry(LC_LOAD_DYLIB,               "load a dynamically linked shared library"),
    entry(LC_ID_DYLIB,                 "dynamically linked shared lib ident"),
    entry(LC_LOAD_DYLINKER,            "load a dynamic linker"),
    entry(LC_ID_DYLINKER,              "dynamic linker identification"),
    entry(LC_PREBOUND_DYLIB,           "modules prebound for a dynamically"),
    /* linked shared library */
    entry(LC_ROUTINES,                 "image routines"),
    entry(LC_SUB_FRAMEWORK,            "sub framework"),
    entry(LC_SUB_UMBRELLA,             "sub umbrella"),
    entry(LC_SUB_CLIENT,               "sub client"),
    entry(LC_SUB_LIBRARY,              "sub library"),
    entry(LC_TWOLEVEL_HINTS,           "two-level namespace lookup hints"),
    entry(LC_PREBIND_CKSUM,            "prebind checksum"),
    entry(LC_LOAD_WEAK_DYLIB,          "load a dynamically linked shared library that is allowed "
                                       "to be missing (all symbols are weak imported)."),
    entry(LC_SEGMENT_64,               "64-bit segment of this file to be mapped"),
    entry(LC_ROUTINES_64,              "64-bit image routines"),
    entry(LC_UUID,                     "the uuid"),
    entry(LC_RPATH,                    "runpath additions"),
    entry(LC_CODE_SIGNATURE,           "local of code signature"),
    entry(LC_SEGMENT_SPLIT_INFO,       "local of info to split segments"),
    entry(LC_REEXPORT_DYLIB,           "load and re-export dylib"),
    entry(LC_LAZY_LOAD_DYLIB,          "delay load of dylib until first use"),
    entry(LC_ENCRYPTION_INFO,          "encrypted segment information"),
    entry(LC_DYLD_INFO,                "compressed dyld information"),
    entry(LC_DYLD_INFO_ONLY,           "compressed dyld information only"),
    entry(LC_LOAD_UPWARD_DYLIB,        "load upward dylib"),
    entry(LC_VERSION_MIN_MACOSX,       "build for MacOSX min OS version"),
    entry(LC_VERSION_MIN_IPHONEOS,     "build for iPhoneOS min OS version"),
    entry(LC_FUNCTION_STARTS,          "compressed table of function start addresses"),
    entry(LC_DYLD_ENVIRONMENT,         "string for dyld to treat like environment variable"),
    entry(LC_MAIN,                     "replacement for LC_UNIXTHREAD"),
    entry(LC_DATA_IN_CODE,             "table of non-instructions in __text"),
    entry(LC_SOURCE_VERSION,           "source version used to build binary"),
    entry(LC_DYLIB_CODE_SIGN_DRS,      "Code signing DRs copied from linked dylibs"),
    entry(LC_ENCRYPTION_INFO_64,       "64-bit encrypted segment information"),
    entry(LC_LINKER_OPTION,            "linker options in MH_OBJECT files"),
    entry(LC_LINKER_OPTIMIZATION_HINT, "optimization hints in MH_OBJECT files"),
    entry(LC_VERSION_MIN_TVOS,         "build for AppleTV min OS version"),
    entry(LC_VERSION_MIN_WATCHOS,      "build for Watch min OS version"),
    entry(LC_NOTE,                     "arbitrary data included within a Mach-O file"),
    entry(LC_BUILD_VERSION,            "build for platform min OS version"),
    entry(LC_DYLD_EXPORTS_TRIE,        "used with linkedit_data_command, payload is trie"),
    entry(LC_DYLD_CHAINED_FIXUPS,      "used with linkedit_data_command"),
    entry(LC_FILESET_ENTRY,            "used with fileset_entry_command")
};
// all `const_info_t` were defined, no longer need this macro
#undef entry

/**********************/
/** Helper Functions **/
/**********************/

/*
 * Compute the minimum (min) or maximum (max) of two integers without branching
 * Reference: https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
 */
static inline uintmax_t min(uintmax_t x, uintmax_t y)
{
    return y ^ ((x ^ y) & -(x < y));
}

/*
 * Round up to the next power of 2
 * Reference: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
 */
static inline uintmax_t next_power_of_2(uintmax_t v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    if (sizeof(uintmax_t) > 1) v |= v >> 8;
    if (sizeof(uintmax_t) > 2) v |= v >> 16;
    if (sizeof(uintmax_t) > 4) v |= v >> 32;
    if (sizeof(uintmax_t) > 8) v |= v >> 64;
    /* round-down when v == UINTMAX_MAX */
    v >>= (uintmax_t)(v == UINTMAX_MAX);
    v += UINTMAX_C(1);
    return v;
}

/*
 * Round up to the next multiple of `aligment`
 */
static inline uintmax_t align(uintmax_t val, uintmax_t alignment)
{
    alignment |= (uintmax_t)(alignment == 0);
    if (alignment < (UINTMAX_MAX - val))
        val += alignment - 1;
    return (val / alignment) * alignment;
}

/*
 * fills the first `n` characters of the array pointed 
 * to by `dst` with the constant byte `c`.
 */
void *memset (void *dst CC_nonnull, int c, size_t n)
{
  char *s = (char*) dst;
  while (n--) *s++ = (char)c;
  return dst;
}

/* 
 * compute the length of the string `str` or until the
 * maximum `n` number of characters have been inspected 
 */
size_t strnlen(const char *str CC_nonnull, size_t n)
{
	const char *start = str;
    for (; n-- && *str; str++);
    return str - start;
}

/* 
 * compute the length of the string `str` until it reaches a <NULL> character
 */
size_t strlen(const char *str CC_nonnull)
{
	const char *start = str;
	for (; *str; str++);
	return str - start;
}

/* repeat `c` character `n` times  */
static inline char *repeat(char *dest, const char c, size_t n)
{
    while (n--) *dest++ = c;
    return dest;
}

/* 
 * appends not more than `n` characters from the string `src` (including the
 * terminating null character) to the end of the string `dst`. The initial
 * character of `src` overwrites the null character at the end of `dst`. A
 * terminating null character is always appended to the result.
 */
char *strncpy(char *CC_restrict dest, const char *CC_restrict src, size_t len)
{
    for (;len > 0 && (*dest = *src++) != 0; len--, dest++);
    return dest;
}

/* 
 * appends not more than `n` characters from the string `src` (including the
 * terminating null character) to the end of the string `dst`. The initial
 * character of `src` overwrites the null character at the end of `dst`. A
 * terminating null character is always appended to the result.
 */
char *strncat(char* CC_restrict dest, const char* CC_restrict src, size_t n)
{
	char *s = dest;
	while (*dest) dest++;
    while (n && *src) n--, *dest++ = *src++;
    *dest++ = 0;
    return s;
}

/* 
 * writes `n` bytes from uint `x` to the array pointed to by `dest`
 * in little-endian hexadecimal format.
 */
char *int2hex_le(uintmax_t x, int n, char* CC_nonnull dest)
{
    const char *xdigits = "0123456789ABCDEF";
    char *end = dest + (n << 1);
    while (dest < end)
    {
        *dest++ = xdigits[((x>>4)&15)];
        *dest++ = xdigits[((x>>0)&15)];
        x >>= 8;
    }
    return end;
}

/*
 * writes `n` bytes from uint `x` to the array pointed to by `dest`
 * in big-endian hexadecimal format.
 */
char *int2hex_be(uintmax_t x, int n, char *dest)
{
    const char *xdigits = "0123456789ABCDEF";
    n <<= 1;
    for (char *end = dest + n; end > dest; x >>= 4) *--end = xdigits[(x & 15)];
    return dest + n;
}

/* 
 * writes `n` bytes in hexadecimal from the array pointed 
 * to by `src` the array pointed to by `dest`.
 */
char *bytes2hex(char* CC_restrict dest, bytes_t CC_restrict src, size_t n)
{
    const char *xdigits = "0123456789ABCDEF";
    char *end = dest + (n << 1);
    unsigned char x;
    while (dest < end)
    {
        x = (unsigned char)*src++;
        *dest++ = xdigits[((x>>4)&15)];
        *dest++ = xdigits[((x>>0)&15)];
    }
    return end;
}

/* copy string src to dest */
char *fmt_s(char* CC_restrict dest, const char* CC_restrict src)
{
	while ((*dest=*src) != 0) src++, dest++;
	return dest;
}

/* format unsigned integer to decimal */
char *fmt_u(char *out, uintmax_t x)
{
    uintmax_t y = x;
    char *p = out;
#if UINTMAX_MAX > UINT64_MAX
    if (y >= UINTMAX_C(10000000000000000) && (p = p + 16) > out)
        y /= UINTMAX_C(10000000000000000);
    if (y >= UINTMAX_C(10000000000000000) && (p = p + 16) > out)
        y /= UINTMAX_C(10000000000000000);
#endif
#if UINTMAX_MAX > UINT32_MAX
    if (y >= UINTMAX_C(10000000000) && (p = p + 10) > out)
        y /= UINTMAX_C(10000000000);
#endif
#if UINTMAX_MAX > UINT16_MAX
    if (y >= UINTMAX_C(100000) && (p = p + 5) > out)
        y /= UINTMAX_C(100000);
#endif
    y = (((y + UINTMAX_C(393206)) & (y + UINTMAX_C(524188))) ^
         ((y + UINTMAX_C(916504)) & (y + UINTMAX_C(514288)))) >> 17;
    p = p + (y + 1);
    if (p < out) return out;
	for (out=p; x>=10; x/=10) *--out = '0' + x%10;
	*--out = '0' + x;
	return p;
}

/* format the fixed size hexadecimal column. */
char *fmt_bytes_column(char* out, bytes_t src, size_t srclen)
{
    srclen = (size_t)min((uintmax_t)srclen, UINTMAX_C(8));
    out = bytes2hex(out, src, srclen);
    out = repeat(out, ' ', 17 - (srclen << 1));
    *(out++) = ';';
    return out;
}

/* format unsigned integer to decimal */
char *fmt_row(char *out, const char *name, bytes_t src, size_t srclen)
{
    size_t len;
    /* first column: value raw bytes in hexadecimal,
     * OBS: maximum of 8 bytes per row. */
    len = (size_t)min((uintmax_t)srclen, UINTMAX_C(8));
    out = fmt_bytes_column(out, src, len);
    src += len;
    srclen -= len;

    /* align `name` to right */
    len = 0;
    if (name && name[0] != 0) len = strlen(name);
    out = repeat(out, ' ', 23 - (size_t)min((uintmax_t)len, UINTMAX_C(22)));

    /* second column: attribute name */
    if (len > 0)
        out = strncpy(out, name, len);

    /* when srclen > 8, create more rows to accommodate 
     * remaining bytes, leave the second column empty */
    while (srclen > 0) {
        *(out++) = '\n';
        len = (size_t)min((uintmax_t)srclen, UINTMAX_C(8));
        out = fmt_bytes_column(out, src, len);
        src += len;
        srclen -= len;
    }
    return out;
}

/* convert bytes to unsigned integer */
uintmax_t bytes2uint(bytes_t bytes, size_t len)
{
    uintmax_t val = 0;
    bytes += len;
    while (len-- > 0)
        val = (val << 8) | (((uintmax_t)*--bytes) & UINTMAX_C(0xFF));
    return val;
}

/* decode semantic version, formated as xx.y.z */
char *version2str(char *out, uint32_t version)
{
    uint8_t minor, patch;
    patch = ((uint8_t)version) & UINT8_C(0xFF);
    version >>= 8;
    minor = ((uint8_t)version) & UINT8_C(0xFF);
    version >>= 8;
    out = fmt_u(out, (uintmax_t)version);
    *out++ = '.';
    out = fmt_u(out, (uintmax_t)minor);
    if (patch > 0) {
        *out++ = '.';
        out = fmt_u(out, (uintmax_t)patch);
    }
    return out;
}

/* Display error message then exit */
CC_noreturn void fail(int code, const char *err, ...);
void fail(int code, const char *err, ...)
{
    char message[1024] = "[ERROR] ";
    ssize_t len = sizeof("[ERROR] ") - 1;
    va_list args;
    va_start(args, err);
    vsnprintf(message + len, sizeof(message) - 1 - len, err, args);
    va_end(args);
    message[sizeof(message) - 1] = 0;
    fflush(stdout);         /* flush stdout */
    fputs((const char*)message, stderr); /* write message to stderr */
    fflush(stderr);         /* flush stderr */
    do { exit((int)code); } while(1);
}

/* Fast Integer Exponentiation Algorithm */
uint32_t pow_u32(uint32_t b, uint32_t e)
{
    uint32_t r = 1;
    while (e > 0) {
        if (e & 1)
            r = r * b;
        e = e >> 1;
        b = b * b;
    }
    return r;
}

/*
 * Sanitize a string by converting ascii-control and non-ascii characters
 * to a printf format-like string, raw bytes are converted to octal.
 * 
 * ptr: input strng
 * len: input string length in bytes.
 * out: output string (must be at least 4x greater than the input string).
 */
char *sanitize_string(const char *src, size_t len, char *out)
{
    char ch;
    const char* end = src + len;

    /* move the end pointer end of the string */
    while (end > src && *(--end) == 0);
    end++;

    /* loop over each input char. */
    while (src < end)
    {
        ch = *src;
        src = src + 1;
        if (ch >= 0x20 && ch < 127) {
            /* escape printf special format chars */
            switch (ch) {
                case '\"': /* 0x22 (quotation mark) */
                case '\'': /* 0x27 (apostrophee) */
                case '\?': /* 0x3F (question mask) */
                case '\\': /* 0x5C (backlash) */
                    *(out++) = '\\';
                    break;
                case '%':  /* 0x25 (percent sign) */
                    *(out++) = '%';
                    break;
            }
            *(out++) = ch;
        } else {
            /* escape ascii-control chars and raw bytes */
            *(out++) = '\\';
            switch (ch)
            {
                case '\a': *(out++) = 'a';  break; /* 0x07 Bell, Alert */
                case '\b': *(out++) = 'b';  break; /* 0x08 Backspace */
                case '\t': *(out++) = 't';  break; /* 0x09 Horizontal Tab */
                case '\n': *(out++) = 'n';  break; /* 0x0A Line Feed */
                case '\v': *(out++) = 'v';  break; /* 0x0B Vertical Tab */
                case '\f': *(out++) = 'f';  break; /* 0x0C Form Feed */
                case '\r': *(out++) = 'r';  break; /* 0x0D Carriage Return */
                default : {
                    /* non-printable bytes are converted to 3-digit
                     * octal format, ex: byte 0x7F become "\177" */
                    *(out++) = ((ch >> 6) & 3) + '0';
                    *(out++) = ((ch >> 3) & 7) + '0';
                    *(out++) = ((ch >> 0) & 7) + '0';
                }
            }
        }
    }
    *out = 0;
    return out;
}


/* Encodes an integer with `rawlen` bytes to Hex0 */
char *field_int2hex0(char *out, const char *fieldname, bytes_t raw, int rawlen)
{
    uint64_t val = 0;
    bytes_t ptr = raw + rawlen;
    while (ptr > raw) {
        val <<= sizeof(raw[0]) << 3;
        val |= ((uint64_t)*--ptr) & UINT64_C(0xFF);
    }
    out = fmt_row(out, fieldname, raw, rawlen);
    out = fmt_u(out, val);
    *(out++) = '\n';
    return out;
}

/* Encode a field with constant value to Hex0 */
char *field_val2hex0(
    char* CC_restrict out,
    const char* CC_restrict fieldname,
    const char* CC_restrict value,
    bytes_t CC_restrict bytes,
    size_t bytelen
) {
    size_t len = (size_t)min((uintmax_t)bytelen, UINTMAX_C(8));
    char padding[32];
    out = fmt_row(out, fieldname, bytes, len);
    out = fmt_s(out, value);
    *(out++) = '\n';
    *(fmt_s(fmt_u(fmt_s((char*)padding, "(pad to "), (uintmax_t)bytelen), " bytes) ")) = 0;
    padding[sizeof(padding)-2] = '\n';
    padding[sizeof(padding)-1] = 0;
    bytes += len;
    bytelen -= len;
    while (bytelen > 0) {
        len = (size_t)min((uintmax_t)bytelen, UINTMAX_C(8));
        out = fmt_bytes_column(out, bytes, len);
        /* display (pad to X bytes) when string length is multiple
         * of 8 and this line contains trailing zeros. */
        if (len == 8 && (bytelen & 7) == 0 && bytes[7] == 0) {
            out = repeat(out, ' ', 23);
            out = fmt_s(out, (const char*)padding);
        }
        *out++ = '\n';
        bytes += len;
        bytelen -= len;
    }
    return out;
}

/* Encodes cstring to Hex0  */
char *field_str2hex0(char *out, const char *fieldname, bytes_t bytes, size_t bytelen)
{
    size_t len = (size_t)min((uintmax_t)bytelen, UINTMAX_C(8));
    char padding[32];
    out = fmt_row(out, fieldname, bytes, len);
    *(out++) = '"';
    out = sanitize_string((const char *)bytes, bytelen, out);
    *(out++) = '"';
    *(out++) = '\n';
    *(fmt_s(fmt_u(fmt_s((char*)padding, "(pad to "), (uintmax_t)bytelen), " bytes) ")) = 0;
    padding[sizeof(padding)-2] = '\n';
    padding[sizeof(padding)-1] = 0;
    bytes += len;
    bytelen -= len;
    while (bytelen > 0) {
        len = (size_t)min((uintmax_t)bytelen, UINTMAX_C(8));
        out = fmt_bytes_column(out, bytes, len);
        /* display (pad to X bytes) when string length is multiple
         * of 8 and this line contains trailing zeros. */
        if (len == 8 && (bytelen & 7) == 0 && bytes[7] == 0) {
            out = repeat(out, ' ', 23);
            out = fmt_s(out, (const char*)padding);
        }
        *out++ = '\n';
        bytes += len;
        bytelen -= len;
    }
    return out;
}

/* writes a section title to hex0 */
char *title2hex0(char *out, const char *str, size_t len)
{
    size_t margin;
    if (len == 0 && str != NULL)
        len = strlen(str);
    /* compute the length of the sanitized string */
    len = margin = (size_t)(sanitize_string(str, len, out) - out);
    out = fmt_s(out, "                 ; -- ");
    /* centralize */
    margin += 7;
    margin = margin >= 22 ? 0 : 22 - margin;
    out = repeat(out, ' ', margin>>1);
    out = sanitize_string(str, len, out);
    out = repeat(out, ' ', (margin+1)>>1);
    out = fmt_s(out, " --\n");
    return out;
}

/* Retrieves the constant name, if not found returns NULL */
static const char* get_entry_name(const_info_t *array, size_t len, uint32_t id)
{
    size_t i;
    for (i = 0; i < len; i++)
        if (array[i].value == id)
            return array[i].name;
    return NULL;
}

/* Retrieves the constant name, if not found returns "<UNKNOWN>" */
inline static const char* get_entry_name_or_default(const_info_t *array, size_t len, uint32_t id)
{
    const char *name;
    if ((name = get_entry_name(array, len, id))) return name;
    else return "<UNKNOWN>";
}

/*****************************/
/** Mach-O Parser Functions **/
/*****************************/

/*
 * Mach-o Parser Entrypoint.
 *
 * - buffer: raw mach-o file bytes
 * - len: file length in bytes
 */
int parser_init(mach_decoded_t *p, bytes_t src, size_t len)
{
    uint32_t i, sizeofcmds;
    const uint32_t *ptr;
    struct load_command *cmd;
    if (len < sizeof(struct mach_header)) {
        printf("len < sizeof(struct mach_header)...\n");
        fflush(stdout);
        return -1;
    }
    ptr = (const uint32_t *)src;

    // Parse Mach-O Header
    p->header.magic = *ptr++;
    p->header.cputype = *ptr++;
    p->header.cpusubtype = *ptr++;
    p->header.filetype = *ptr++;
    p->header.ncmds = *ptr++;
    p->header.sizeofcmds = *ptr++;
    p->header.flags = *ptr++;

    switch (p->header.magic)
    {
        case MH_MAGIC_64:
        case MH_CIGAM_64:
            p->header.reserved = *ptr++;
            break;
        case MH_MAGIC:
        case MH_CIGAM:
            p->header.reserved = 0;
            break;
        default:
            printf("invalid magic: %"PRIx32"\n", p->header.magic);
            fflush(stdout);
            return -1;
            break;
    }
    p->src = ptr;
    p->len = len - (((uintptr_t)ptr) - ((uintptr_t)src));

    fflush(stdout);

    if (p->header.sizeofcmds > p->len
        || p->header.sizeofcmds < (p->header.ncmds * sizeof(struct load_command))
        || p->header.ncmds > NCMDS_MAX)
        return -1;
    
    // Parse Load Commands
    i = sizeofcmds = 0;
    while (i < p->header.ncmds)
    {
        cmd = (struct load_command *)ptr;

        if (cmd->cmdsize < sizeof(struct load_command)
            || sizeofcmds >= (sizeofcmds + cmd->cmdsize))
            return -1;
        sizeofcmds += cmd->cmdsize;
        p->load_commands[i++] = cmd;
        ptr = (const uint32_t*)(((uintptr_t)ptr) + cmd->cmdsize);
    }
    for (; i < NCMDS_MAX; ++i)
        p->load_commands[i] = NULL;

    return 0;
}


/* Encodes Mach-O Header to Hex0  */
char *header2hex0(const mach_decoded_t *p, char *out)
{
    int i;
    const char *val;

    // -- TITLE --
    out = title2hex0(out, "MACH-O HEADER", 0);

    // -- MAGIC --
    val = get_entry_name_or_default(
        mach_magics_s,
        (sizeof(mach_magics_s) / sizeof(mach_magics_s[0])),
        p->header.magic
    );
    out = field_val2hex0(out, "magic: ", val, (bytes_t)&p->header.magic, 4);

    // -- CPU TYPE --
    val = get_entry_name_or_default(
        cputypes_s,
        (sizeof(cputypes_s) / sizeof(cputypes_s[0])),
        p->header.cputype
    );
    out = field_val2hex0(out, "cpu type: ", val, (bytes_t)&p->header.cputype, 4);
    
    // -- CPU SUBTYPE --
    switch (p->header.cputype)
    {
        case CPU_TYPE_VAX:
        val = get_entry_name_or_default(
            cpusubtypes_vax_s,
            (sizeof(cpusubtypes_vax_s) / sizeof(cpusubtypes_vax_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_MC680x0:
        val = get_entry_name_or_default(
            cpusubtypes_mc680x0_s,
            (sizeof(cpusubtypes_mc680x0_s) / sizeof(cpusubtypes_mc680x0_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_X86:
        val = get_entry_name_or_default(
            cpusubtypes_x86_s,
            (sizeof(cpusubtypes_x86_s) / sizeof(cpusubtypes_x86_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_X86_64:
        val = get_entry_name(
            cpusubtypes_x86_64_s,
            (sizeof(cpusubtypes_x86_64_s) / sizeof(cpusubtypes_x86_64_s[0])),
            p->header.cpusubtype
        );
        if (val == NULL)
            val = get_entry_name_or_default(
                cpusubtypes_x86_s,
                (sizeof(cpusubtypes_x86_s) / sizeof(cpusubtypes_x86_s[0])),
                p->header.cpusubtype & (~CPU_SUBTYPE_LIB64)
            );
        break;
        case CPU_TYPE_MC98000:
        val = get_entry_name_or_default(
            cpusubtypes_mc98000_s,
            (sizeof(cpusubtypes_mc98000_s) / sizeof(cpusubtypes_mc98000_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_HPPA:
        val = get_entry_name_or_default(
            cpusubtypes_hppa_s,
            (sizeof(cpusubtypes_hppa_s) / sizeof(cpusubtypes_hppa_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_ARM:
        val = get_entry_name_or_default(
            cpusubtypes_arm32_s,
            (sizeof(cpusubtypes_arm32_s) / sizeof(cpusubtypes_arm32_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_ARM64:
        val = get_entry_name_or_default(
            cpusubtypes_arm64_s,
            (sizeof(cpusubtypes_arm64_s) / sizeof(cpusubtypes_arm64_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_ARM64_32:
        val = get_entry_name_or_default(
            cpusubtypes_arm64_32_s,
            (sizeof(cpusubtypes_arm64_32_s) / sizeof(cpusubtypes_arm64_32_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_MC88000:
        val = get_entry_name_or_default(
            cpusubtypes_mc88000_s,
            (sizeof(cpusubtypes_mc88000_s) / sizeof(cpusubtypes_mc88000_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_SPARC:
        val = get_entry_name_or_default(
            cpusubtypes_sparc_s,
            (sizeof(cpusubtypes_sparc_s) / sizeof(cpusubtypes_sparc_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_I860:
        val = get_entry_name_or_default(
            cpusubtypes_i860_s,
            (sizeof(cpusubtypes_i860_s) / sizeof(cpusubtypes_i860_s[0])),
            p->header.cpusubtype
        );
        break;
        case CPU_TYPE_POWERPC:
        case CPU_TYPE_POWERPC64:
        val = get_entry_name_or_default(
            cpusubtypes_powerpc_s,
            (sizeof(cpusubtypes_powerpc_s) / sizeof(cpusubtypes_powerpc_s[0])),
            p->header.cpusubtype
        );
        break;
        default:
        val = "<UNKNOWN>";
        break;
    }
    out = field_val2hex0(out, "cpu subtype: ", val, (bytes_t)&p->header.cpusubtype, 4);

    // -- FILE TYPE --
    val = get_entry_name_or_default(filetypes_s, (sizeof(filetypes_s) / sizeof(filetypes_s[0])), p->header.filetype);
    out = field_val2hex0(out, "file type: ", val, (bytes_t)&p->header.filetype, 4);

    // -- NCMDS --
    out = field_int2hex0(out, "command count: ", (bytes_t)&p->header.ncmds, 4);

    // -- CMDS SIZE --
    out = field_int2hex0(out, "size of count: ", (bytes_t)&p->header.sizeofcmds, 4);
    out = fmt_s(--out, " bytes\n");

    // -- FLAGS --
    out = fmt_row(out, "flags: ", (bytes_t)&p->header.flags, 4);
    *out = '0';
    *(out+1) = 0;
    for (i = 0; i < (sizeof(headerflags_s) / sizeof(headerflags_s[0])); i++) {
        if (p->header.flags & headerflags_s[i].value) {
            out = fmt_s(out, headerflags_s[i].name);
            out = fmt_s(out, " | ");
        }
    }
    out-=3;
    *(out++) = '\n';

    // -- RESERVED -- (64-bit binaries only)
    switch (p->header.magic)
    {
        case MH_MAGIC_64:
        case MH_CIGAM_64:
            out = field_int2hex0(out, "reserved: ", (bytes_t)&p->header.reserved, 4);
            break;
        default:
            break;
    }
    return out;
}

/* encodes load command's name and size to hex0  */
char *command_header2hex0(char *out, const struct load_command *cmd)
{
    const char *cmdname = get_entry_name_or_default(
        load_commands_s,
        (sizeof(load_commands_s) / sizeof(load_commands_s[0])),
        cmd->cmd
    );
    out = field_val2hex0(out, "command name: ", cmdname, (bytes_t)&cmd->cmd, 4);
    out = field_int2hex0(out, "command size: ", (bytes_t)&cmd->cmdsize, 4);
    return out;
}

/* Encodes LC_SEGMENT_64 to Hex0  */
char *segment_command2hex0(char *out, const struct segment_command_64 *cmd)
{
    ssize_t cmdsize, len;
    const struct section_64 *section;
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_str2hex0(out, "segment name: ", (bytes_t)cmd->segname, 16);
    out = field_int2hex0(out, "vm address: ", (bytes_t)&cmd->vmaddr, 8);
    out = field_int2hex0(out, "vm size: ", (bytes_t)&cmd->vmsize, 8);
    out = field_int2hex0(out, "file offset: ", (bytes_t)&cmd->fileoff, 8);
    out = field_int2hex0(out, "file size: ", (bytes_t)&cmd->filesize, 8);
    out = field_int2hex0(out, "maxprot: ", (bytes_t)&cmd->maxprot, 4);
    out = field_int2hex0(out, "initprot: ", (bytes_t)&cmd->initprot, 4);
    out = field_int2hex0(out, "nsects: ", (bytes_t)&cmd->nsects, 4);
    out = field_int2hex0(out, "flags: ", (bytes_t)&cmd->flags, 4);
    cmdsize = ((ssize_t)cmd->cmdsize) - sizeof(struct segment_command_64);
    section = (struct section_64 *)(((uintptr_t)cmd) + sizeof(struct segment_command_64));
    while (cmdsize >= sizeof(struct section_64))
    {
        /* command sections */
        cmdsize -= sizeof(struct section_64);
        out = title2hex0(out, (const char *)section->sectname, 16);
        out = field_str2hex0(out, "section name: ", (bytes_t)section->sectname, 16);
        out = field_str2hex0(out, "segment name: ", (bytes_t)section->segname, 16);
        out = field_int2hex0(out, "memory address: ", (bytes_t)&section->addr, 8);
        out = field_int2hex0(out, "memory size: ", (bytes_t)&section->size, 8);
        out = field_int2hex0(out, "section offset: ", (bytes_t)&section->offset, 4);
        out = field_int2hex0(out, "section alignment: ", (bytes_t)&section->align, 4); // TODO: pow_u32(2, section->align)
        out = field_int2hex0(out, "relocation offset: ", (bytes_t)&section->reloff, 4);
        out = field_int2hex0(out, "relocation number: ", (bytes_t)&section->nreloc, 4);
        out = field_int2hex0(out, "flags: ", (bytes_t)&section->flags, 4);
        out = field_int2hex0(out, "reserved1: ", (bytes_t)&section->reserved1, 4);
        out = field_int2hex0(out, "reserved2: ", (bytes_t)&section->reserved2, 4);
        out = field_int2hex0(out, "reserved3: ", (bytes_t)&section->reserved3, 4);
        ++section;
    }
    return out;
}

/*
 * Encode DYLD Linker Commands to Hex0.
 * - LC_CODE_SIGNATURE
 * - LC_SEGMENT_SPLIT_INFO
 * - LC_FUNCTION_STARTS
 * - LC_DATA_IN_CODE
 * - LC_DYLIB_CODE_SIGN_DRS
 * - LC_LINKER_OPTIMIZATION_HINT
 * - LC_DYLD_EXPORTS_TRIE
 * - LC_DYLD_CHAINED_FIXUPS
 */
char *linkedit_command2hex0(char *out, const struct linkedit_data_command *cmd)
{
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "data offset: ", (bytes_t)&cmd->dataoff, 4);
    out = field_int2hex0(out, "data size: ", (bytes_t)&cmd->datasize, 4);
    return out;
}

/*
 * Encode LC_SYMTAB command to Hex0.
 */
char *symtab_command2hex0(char *out, const struct symtab_command *cmd)
{
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "symbol table offset: ", (bytes_t)&cmd->symoff, 4);
    out = field_int2hex0(out, "symbol table entries: ", (bytes_t)&cmd->nsyms, 4);
    out = field_int2hex0(out, "string table offset: ", (bytes_t)&cmd->stroff, 4);
    out = field_int2hex0(out, "string table size: ", (bytes_t)&cmd->strsize, 4);
    return out;
}

/*
 * Encode LC_DYSYMTAB command to Hex0.
 */
char *dysymtab_command2hex0(char *out, const struct dysymtab_command *cmd)
{
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "index to localsymbol: ", (bytes_t)&cmd->ilocalsym, 4);
    out = field_int2hex0(out, "number of symbols: ", (bytes_t)&cmd->nlocalsym, 4);
    out = field_int2hex0(out, "iextdefsym: ", (bytes_t)&cmd->iextdefsym, 4);
    out = field_int2hex0(out, "nextdefsym: ", (bytes_t)&cmd->nextdefsym, 4);
    out = field_int2hex0(out, "iundefsym: ", (bytes_t)&cmd->iundefsym, 4);
    out = field_int2hex0(out, "nundefsym: ", (bytes_t)&cmd->nundefsym, 4);
    out = field_int2hex0(out, "tocoff: ", (bytes_t)&cmd->tocoff, 4);
    out = field_int2hex0(out, "ntoc: ", (bytes_t)&cmd->ntoc, 4);
    out = field_int2hex0(out, "module table offset: ", (bytes_t)&cmd->modtaboff, 4);
    out = field_int2hex0(out, "module table entries: ", (bytes_t)&cmd->nmodtab, 4);
    out = field_int2hex0(out, "extrefsymoff: ", (bytes_t)&cmd->extrefsymoff, 4);
    out = field_int2hex0(out, "nextrefsyms: ", (bytes_t)&cmd->nextrefsyms, 4);
    out = field_int2hex0(out, "indirectsymoff: ", (bytes_t)&cmd->indirectsymoff, 4);
    out = field_int2hex0(out, "nindirectsyms: ", (bytes_t)&cmd->nindirectsyms, 4);
    out = field_int2hex0(out, "extreloff: ", (bytes_t)&cmd->extreloff, 4);
    out = field_int2hex0(out, "nextrel: ", (bytes_t)&cmd->nextrel, 4);
    out = field_int2hex0(out, "locreloff: ", (bytes_t)&cmd->locreloff, 4);
    out = field_int2hex0(out, "nlocrel: ", (bytes_t)&cmd->nlocrel, 4);
    return out;
}

/*
 * Encode LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB and LC_REEXPORT_DYLIB commands to Hex0
 */
char *dylib_command2hex0(char *out, const struct dylib_command *cmd)
{
    bytes_t ptr = ((bytes_t)cmd) + sizeof(struct dylib_command);
    size_t len = cmd->cmdsize - sizeof(struct dylib_command);
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "name offset: ", (bytes_t)&cmd->dylib.name, 4);
    out = field_int2hex0(out, "timestamp: ", (bytes_t)&cmd->dylib.timestamp, 4);
    out = field_int2hex0(out, "current version: ", (bytes_t)&cmd->dylib.current_version, 4);
    out = field_int2hex0(out, "compatibility: ", (bytes_t)&cmd->dylib.compatibility_version, 4);
    out = field_str2hex0(out, "name: ", ptr, len);
    return out;
}

/* Encode LC_MAIN command to Hex0 */
char *entry_point_command2hex0(char *out, const struct entry_point_command *cmd)
{
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "offset of main: ", (bytes_t)&cmd->entryoff, 8);
    out = field_int2hex0(out, "initial stack size: ", (bytes_t)&cmd->stacksize, 8);
    return out;
}

/* Encode LC_SOURCE_VERSION command to Hex0 */
char *build_version_command2hex0(char *out, const struct build_version_command *cmd)
{
    char text[32];
    bytes_t ptr;
    uint32_t val, len;

    /* encode command fields */
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "platform: ", (bytes_t)&cmd->platform, 4);
    *(version2str((char*)text, cmd->minos)) = 0;
    out = field_val2hex0(out, "minos: ", (const char*)text, (bytes_t)&cmd->minos, 4);
    *(version2str((char*)text, cmd->sdk)) = 0;
    out = field_val2hex0(out, "sdk: ", (const char*)text, (bytes_t)&cmd->sdk, 4);
    out = field_int2hex0(out, "number of entries: ", (bytes_t)&cmd->ntools, 4);
    
    /* encode entries */
    len = cmd->cmdsize - sizeof(struct build_version_command);
    ptr = ((bytes_t)cmd) + sizeof(struct build_version_command);
    while (len > 0) {
        /* title */
        val = (cmd->ntools + 1) - (len >> 3);
        *(fmt_u(fmt_s((char*)text, "ENTRY "), (uintmax_t)val)) = 0;
        out = title2hex0(out, (const char*)text, 0);

        /* tool */
        val = (uint32_t)min((uintmax_t)len, UINTMAX_C(4));
        out = field_int2hex0(out, "tool: ", ptr, (size_t)val);
        len -= val;
        ptr += val;

        /* version */
        val = (uint32_t)min((uintmax_t)len, UINTMAX_C(4));
        *(version2str((char*)text, (uint32_t)bytes2uint(ptr, (size_t)val))) = 0;
        out = field_val2hex0(out, "version: ", (const char*)text, ptr, (size_t)val);
        len -= val;
        ptr += val;
    }
    return out;
}

/* Encode LC_UUID command to Hex0 */
char *uuid_command2hex0(char *out, const struct uuid_command *cmd)
{
    char uuid[64];
    char *ptr;

    /* encode uuid-v4 */
    memset((void*)uuid, 0, sizeof(uuid));
    ptr = (char*)uuid;
    ptr = bytes2hex(ptr, (bytes_t)&cmd->uuid[0], 4);
    *ptr++ = '-';
    ptr = bytes2hex(ptr, (bytes_t)&cmd->uuid[4], 2);
    *ptr++ = '-';
    ptr = bytes2hex(ptr, (bytes_t)&cmd->uuid[6], 2);
    *ptr++ = '-';
    ptr = bytes2hex(ptr, (bytes_t)&cmd->uuid[8], 2);
    *ptr++ = '-';
    ptr = bytes2hex(ptr, (bytes_t)&cmd->uuid[10], 6);
    *ptr = 0;

    /* encode command fields */
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_val2hex0(out, "uuid: ", (const char*)uuid, (bytes_t)cmd->uuid, sizeof(cmd->uuid));
    return out;
}

/* Encode LC_SOURCE_VERSION command to Hex0 */
char *source_version_command2hex0(char *out, const struct source_version_command *cmd)
{
    // A.B.C.D.E packed as a24.b10.c10.d10.e10
    char *ptr, version[64];
    uint64_t val;
    memset((void*)version, 0, sizeof(version));
    ptr = fmt_u((char*)version, (uintmax_t)(cmd->version>>40));
    val = cmd->version & UINT64_C(0xFFFFFFFFFF);
    while (val > 0) {
        *ptr++ = '.';
        val = val << 10;
        ptr = fmt_u(ptr, (uintmax_t)val>>40);
        val = val & UINT64_C(0xFFFFFFFFFF);
    }

    /* encode command fields */
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_val2hex0(out, "version: ", (const char*)version, (bytes_t)&cmd->version, sizeof(cmd->version));
    return out;
}

/* Encode LC_ID_DYLINKER, LC_LOAD_DYLINKER and LC_DYLD_ENVIRONMENT commands to Hex0 */
char *dylinker_command2hex0(char *out, const struct dylinker_command *cmd)
{
    bytes_t ptr, end;
    size_t len = cmd->cmdsize - cmd->name;
    ptr = ((bytes_t)cmd) + cmd->name;
    end = ((bytes_t)cmd) + cmd->cmdsize;
    /* encode command fields */
    out = command_header2hex0(out, (const struct load_command*)cmd);
    out = field_int2hex0(out, "name offset: ", (bytes_t)&cmd->name, 4);
    if (ptr >= (((bytes_t)cmd) + sizeof(struct dylinker_command)) && (ptr+len) <= end)
        out = field_str2hex0(out, "name: ", ptr, len);
    return out;
}

/* Encode Load Command to Hex0  */
char *load_command2hex0(char *out, const struct load_command *cmd)
{
    const char *val;
    uint32_t cmdsize;

    switch (cmd->cmd)
    {
    case LC_SEGMENT_64:
        return segment_command2hex0(out, (const struct segment_command_64*)cmd);
    case LC_CODE_SIGNATURE:
    case LC_SEGMENT_SPLIT_INFO:
    case LC_FUNCTION_STARTS:
    case LC_DATA_IN_CODE:
    case LC_DYLIB_CODE_SIGN_DRS:
    case LC_LINKER_OPTIMIZATION_HINT:
    case LC_DYLD_EXPORTS_TRIE:
    case LC_DYLD_CHAINED_FIXUPS:
        return linkedit_command2hex0(out, (const struct linkedit_data_command*)cmd);
    case LC_SYMTAB:
        return symtab_command2hex0(out, (const struct symtab_command*)cmd);
    case LC_DYSYMTAB:
        return dysymtab_command2hex0(out, (const struct dysymtab_command*)cmd);
    case LC_LOAD_DYLIB:
    case LC_REEXPORT_DYLIB:
    case LC_LOAD_WEAK_DYLIB:
        return dylib_command2hex0(out, (const struct dylib_command*)cmd);
    case LC_MAIN:
        return entry_point_command2hex0(out, (const struct entry_point_command*)cmd);
    case LC_SOURCE_VERSION:
        return source_version_command2hex0(out, (const struct source_version_command*)cmd);
    case LC_BUILD_VERSION:
        return build_version_command2hex0(out, (const struct build_version_command*)cmd);
    case LC_UUID:
        return uuid_command2hex0(out, (const struct uuid_command*)cmd);
    case LC_ID_DYLINKER:
    case LC_LOAD_DYLINKER:
    case LC_DYLD_ENVIRONMENT:
        return dylinker_command2hex0(out, (const struct dylinker_command*)cmd);
    default:
        break;
    }

    // -- COMMAND NAME AND SIZE --
    out = command_header2hex0(out, cmd);

    // -- REST --
    val = (const char *)(((uintptr_t)cmd) + sizeof(struct load_command));
    cmdsize = cmd->cmdsize - sizeof(struct load_command);
    while (cmdsize > 0) {
        if (cmdsize >= 8) {
            out = fmt_row(out, "", (bytes_t)val, 8);
            *(out++) = '\n';
            val = val + 8;
            cmdsize = cmdsize - 8;
        } else {
            out = fmt_row(out, "", (bytes_t)val, (int)cmdsize);
            *(out++) = '\n';
            val = val + cmdsize;
            cmdsize = 0;
            break;
        }
    }
    return out;
}

/*
 * Mach-o Parser Entrypoint.
 *
 * - buffer: raw mach-o file bytes
 * - len: file length in bytes
 */
int parse(uint8_t *buffer, size_t len)
{
    uint32_t cmd_count, cmd_size;
    ssize_t ret;
    struct mach_header* header = NULL;
    mach_decoded_t p;
    char buf[8192];
    char *out, *title_ptr;
    if ((ret = parser_init(&p, buffer, len)))
        return (int)ret;

    out = (char *)buf;
    out = header2hex0(&p, out);
    *out = 0;
    fputs(buf, stdout);
    fflush(stdout);
    cmd_count = 0;
    while (cmd_count < p.header.ncmds)
    {
        ++cmd_count;
        // -- TITLE --
        // use the last 128 bytes of the buffer to format the title
        out = (char*)(((uintptr_t)buf) + sizeof(buf) - 128);
        out = (char*)align((uintmax_t)out, 16);
        // save string pointed to by title string
        title_ptr = out;
        // format title
        out = fmt_s(out, "LOAD COMMAND ");
        out = fmt_u(out, (uintmax_t)cmd_count);
        *out = 0;

        // reset pointer and write title
        out = (char*)buf;
        *(out++) = '\n';
        out = title2hex0(out, (const char *)title_ptr, 0);

        // -- COMMAND --
        out = load_command2hex0(out, p.load_commands[(cmd_count-1)]);
        *out = 0;
        fputs(buf, stdout);
        fflush(stdout);
    }
    return 0;
}

/* display usage message then exit. */
int usage(const char* cmd) {
    int ret, len;
    char cmdname[64];
    const char *end;
    
    if (cmd == NULL
        || (len = (int)strlen(cmd)) <= 0
        || *(end = cmd + len) != 0
        || *(--end) == '/')
    {
        cmd = "Mach2Hex0";
        len = sizeof("Mach2Hex0") - 1;
    }
    end = cmd;
    cmd = cmd + len;
    for (; cmd >= end && *cmd != '/'; --cmd);
    ++cmd;
    len = (int)((end + len) - cmd);

    if (len < sizeof(cmdname)) ret = snprintf(cmdname, sizeof(cmdname), "%.*s", len, cmd);
    else ret = snprintf(cmdname, sizeof(cmdname), "%.*s...", (int)(sizeof(cmdname) - 4), cmd);
    if (ret <= 0) fail(1, "snprintf failed");
    if (ret >= sizeof(cmdname)) fail(1, "snprintf out of bounds");

    ret = printf(
        "`%.*s\' converts a Mach-O (MacOS) executable to Hex0 format\n\n"
        "Usage: %.*s <FILE_PATH>\n\n"
        "Defaults for the options are specified in brackets.\n\n"
        "Options:\n"
        "-h, --help              display this help and exit\n",
        len, cmdname,
        len, cmdname
    );
    fflush(stdout);
    ret = ret > 0 ? 0 : -ret;
    exit(ret);
    return ret;
}

/* Mach2Hex0 main */
int main(int argc, char *argv[]) {
    char *cmd = "";
    FILE *file = NULL;
    uint8_t buffer[4096];
    size_t len, pathlen;
    struct stat st;

    /* Validate arguments */
    if (argc && argv && argv[0])
        cmd = argv[0];

    if (argc < 2 || !argv || !argv[1])
        return usage(argv ? argv[0] : NULL);
    if (argc > 3)
        fail(1, "extra operand\n");

    /* check filepath */
    cmd = argv[argc - 1];
    pathlen = strnlen(cmd, sizeof(buffer));
    if (pathlen >= sizeof(buffer))
        fail(1, "argument exceed maximum size of 4096 bytes\n");
    
    if (pathlen > PATH_MAX)
        fail(1, "file path exceeds maximum size of "TOSTRING(PATH_MAX)"\n");

    /* sanitize filepath before print on console. */
    len = pathlen < sizeof(buffer) ? pathlen : sizeof(buffer) - 1;
    len = (size_t)(sanitize_string(argv[1], len, (char*)buffer) - argv[1]);
    buffer[len] = 0;

    if (stat(argv[1], &st) != 0)
        fail(1, "file not found \"%.*s\"\n", (int)len, buffer);
    if (!S_ISREG(st.st_mode))
        fail(1, "not a file \"%.*s\"\n", (int)len, buffer);
    if (st.st_size < sizeof(buffer))
        fail(1, "less than 4096 bytes in size \"%.*s\"\n", (int)len, buffer);
    if ((file = fopen(argv[1], "rb")) == NULL)
        fail(1, "cannot open file \"%.*s\"\n", (int)len, buffer);

    /* print the filename and size in bytes */
    printf(
        "                 ; FILE NAME: \"%.*s\"\n"
        "                 ; FILE SIZE: %"PRIu64" bytes\n"
        "\n",
        (int)len, buffer,
        (uint64_t)st.st_size);

    /* read the first 4096 bytes from file (Mach-o minimum size) */
    len = fread(buffer, 1, sizeof(buffer) - 1, file);
    if (fclose(file) != 0)
        fail(2, "[ERROR] %s: cannot close file '%s'\n", cmd, argv[1]);
    if (len == 0 || len >= sizeof(buffer))
        fail(2, "[ERROR] %s: failed to read file '%s'\n", cmd, argv[1]);

    /* Make sure the buffer ends with zero */
    buffer[len] = 0;

    /* Flush any pending print */
    fflush(stdin);
    /* start parser */
    return parse(buffer, len);
}
