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
#include <string.h>
#include <stdarg.h>
#include "mach2hex0.h"

/******************************/
/* Detect C Compiler Features */
/******************************/

/* Detect C Compiler '__has_builtin' support */
#if defined(__has_builtin)
#define CC_has_builtin(builtin) __has_builtin(__builtin_##builtin)
#else
#define CC_has_builtin(builtin) 0
#endif

/* Detect C Compiler '__has_attribute' builtin support */
#if defined(__has_attribute)
#define CC_has_attribute(attribute) __has_attribute(attribute)
#else
#define CC_has_attribute(attribute) 0
#endif

/* Detect C Compiler 'noreturn' attribute support */
#if __STDC_VERSION__ >= 202311L
#define CC_noreturn [[noreturn]]
#elif __STDC_VERSION__ >= 201112L
#define CC_noreturn _Noreturn
#elif CC_has_attribute(noreturn) || defined(__GNUC__) || defined(__TINYC__)
#define CC_noreturn __attribute__((noreturn))
#elif defined(_MSC_VER)
#define CC_noreturn __declspec(noreturn)
#else
#define CC_noreturn
#endif

/* Detect C Compiler '__builtin_bswap16' support */
#if CC_has_builtin(bswap16) || defined(__GNUC__)
#define CC_bswap16(val) __builtin_bswap16((uint16_t)val)
#else
static inline uint16_t CC_bswap16(uint16_t val) {
    return (uint16_t)((val & UINT16_C(0xFF00)) >> 8) |
           (uint16_t)((val & UINT16_C(0x00FF)) << 8);
}
#endif

/* Detect C Compiler '__builtin_bswap32' support */
#if CC_has_builtin(bswap32) || defined(__GNUC__)
#define CC_bswap32(val) __builtin_bswap32((uint32_t)val)
#else
static inline uint32_t CC_bswap32(uint32_t val) {
    return
    (uint32_t)CC_bswap16((uint16_t)((val & UINT32_C(0x0000FFFF)) >>  0)) << 16 |
    (uint32_t)CC_bswap16((uint16_t)((val & UINT32_C(0xFFFF0000)) >> 16)) >>  0;
}
#endif

/* Detect C Compiler '__builtin_bswap64' support */
#if CC_has_builtin(bswap64) || defined(__GNUC__)
#define CC_bswap64(val) __builtin_bswap64((uint64_t)val)
#else
static inline uint64_t CC_bswap64(uint64_t val) {
    return 
    (uint64_t)CC_bswap32((uint32_t)((val & UINT64_C(0x00000000FFFFFFFF)) >>  0)) << 32 |
    (uint64_t)CC_bswap32((uint32_t)((val & UINT64_C(0xFFFFFFFF00000000)) >> 32)) >>  0;
}
#endif

/* Helpers for conversion between little-endian <=> big-endian
 * OBS: Assumes mach-o components are encoded in little-endian */
#if defined(_MSC_VER) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define to_u16be(val) CC_bswap16(val)
#define to_u32be(val) CC_bswap32(val)
#define to_u64be(val) CC_bswap64(val)
#define to_u16le(val) ((uint16_t)(val))
#define to_u32le(val) ((uint32_t)(val))
#define to_u64le(val) ((uint64_t)(val))
#else
#define to_u16be(val) ((uint16_t)(val))
#define to_u32be(val) ((uint32_t)(val))
#define to_u64be(val) ((uint64_t)(val))
#define to_u16le(val) CC_bswap16(val)
#define to_u32le(val) CC_bswap32(val)
#define to_u64le(val) CC_bswap64(val)
#endif

/************************/
/* structs and typedefs */
/************************/
/* Store the string name of a raw Mach-O Constant */
typedef struct const_info_t {
    uint32_t value;
    const char *name;
    const char *desc;
} const_info_t;

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
    entry(MH_NOUNDEFS,                      "the object file has no undefined references"),
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
/* Display error message then exit and exit */
CC_noreturn void fail(int code, const char *err, ...);
void fail(int code, const char *err, ...)
{
    va_list args;
    vfprintf(stderr, err, args);
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

/* Check if 'ch' is a visible ascii char */
inline static int is_ascii(const char ch) {
    return ch >= 0x20 && ch < 127;
}

/*
 * Sanitize a string by converting ascii-control and non-ascii characters
 * to a printf format-like string, raw bytes are converted to octal.
 * 
 * ptr: input strng
 * len: input string length in bytes.
 * out: output string (must be at least 4x greater than the input string).
 */
char* sanitize_string(const char *ptr, const size_t len, char *out)
{
    const char* end;
    char ch;
    {
        /* equivalent to: memset(out, 0, len * 4) */
        uint32_t *p = (uint32_t*)out;
        end = (char*)(p + len);
        while (((char*)p) < end) *(p++) = 0;
    }
    end = ptr + len;

    /* move the end pointer right after the last non-zero byte */
    while (end > ptr && (*(end-1) == 0)) end = end - 1;

    /* move the end pointer right after the last non-zero byte */
    while (ptr < end)
    {
        ch = *ptr;
        ptr = ptr + 1;
        if (is_ascii(ch)) {
            /* Escape following ascii chars
             * \ (backlash)
             * " (quotation mark)
             * ' (apostrophee)
             * ? (question mask)
             * % (percent sign) */
            switch (ch) {
                case '\\':
                case '\"':
                case '\'':
                case '\?':
                    *(out++) = '\\';
                    break;
                case '%':
                    *(out++) = '%';
                    break;
            }
            *(out++) = ch;
        } else {
            /* Escape ascii-control chars and raw bytes */
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
                    /* 
                     * Remaining bytes are converted to 3-digit octal format,
                     * ex: byte 0x7F become "\177"
                     */
                    *(out++) = ((ch >> 6) & 3) + '0';
                    *(out++) = ((ch >> 3) & 7) + '0';
                    *(out++) = ((ch >> 0) & 7) + '0';
                }
            }
        }
    }
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

/* Retrieves the Load Command Name, if not found returns "<UNKNOWN>" */
static const char* id2lc_command_name(uint32_t id)
{
    return get_entry_name_or_default(
        load_commands_s,
        (sizeof(load_commands_s) / sizeof(load_commands_s[0])),
        id
    );
}

/*****************************/
/** Mach-O Parser Functions **/
/*****************************/

/* Parses the Mach-o Header
 * TODO: add support to 32-bit headers */
ssize_t parse_mach_header(const uint8_t *buffer, size_t len, struct mach_header** out)
{
    int i;
    size_t flaglen, cpu_subtypes_len;
    struct mach_header_64* header;
    const char *magic, *cputype, *cpusubtype, *filetype;
    char *headerflags, *ptr;
    const_info_t *cpu_subtypes_arr;

    // buffer must have at minium Mach-O Header size
    if (!buffer || !out || len < sizeof(struct mach_header_64))
        return ~1;

    // MAGIC
    header = (struct mach_header_64*) buffer;
    switch (header->magic)
    {
        case MH_MAGIC_64:
        case MH_CIGAM_64:
            len = sizeof(struct mach_header_64);
            break;
        case MH_MAGIC:
        case MH_CIGAM:
            len = sizeof(struct mach_header);
            break;
        default:
            fail(1, "invalid mach-o magic: %08"PRIx32, to_u32be(header->magic));
    }
    magic = get_entry_name_or_default(
        mach_magics_s,
        (sizeof(mach_magics_s) / sizeof(mach_magics_s[0])),
        header->magic
    );
    
    // CPU TYPE
    cputype = get_entry_name_or_default(
        cputypes_s,
        (sizeof(cputypes_s) / sizeof(cputypes_s[0])),
        header->cputype
    );
    
    // CPU SUBTYPES
    switch (header->cputype)
    {
        case CPU_TYPE_VAX:
        cpu_subtypes_arr = cpusubtypes_vax_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_vax_s) / sizeof(cpusubtypes_vax_s[0]));
        break;
        case CPU_TYPE_MC680x0:
        cpu_subtypes_arr = cpusubtypes_mc680x0_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_mc680x0_s) / sizeof(cpusubtypes_mc680x0_s[0]));
        break;
        case CPU_TYPE_X86:
        cpu_subtypes_arr = cpusubtypes_x86_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_x86_s) / sizeof(cpusubtypes_x86_s[0]));
        break;
        case CPU_TYPE_X86_64:
        cpu_subtypes_arr = cpusubtypes_x86_64_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_x86_64_s) / sizeof(cpusubtypes_x86_64_s[0]));
        break;
        case CPU_TYPE_MC98000:
        cpu_subtypes_arr = cpusubtypes_mc98000_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_mc98000_s) / sizeof(cpusubtypes_mc98000_s[0]));
        break;
        case CPU_TYPE_HPPA:
        cpu_subtypes_arr = cpusubtypes_hppa_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_hppa_s) / sizeof(cpusubtypes_hppa_s[0]));
        break;
        case CPU_TYPE_ARM:
        cpu_subtypes_arr = cpusubtypes_arm32_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_arm32_s) / sizeof(cpusubtypes_arm32_s[0]));
        break;
        case CPU_TYPE_ARM64:
        cpu_subtypes_arr = cpusubtypes_arm64_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_arm64_s) / sizeof(cpusubtypes_arm64_s[0]));
        break;
        case CPU_TYPE_ARM64_32:
        cpu_subtypes_arr = cpusubtypes_arm64_32_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_arm64_32_s) / sizeof(cpusubtypes_arm64_32_s[0]));
        break;
        case CPU_TYPE_MC88000:
        cpu_subtypes_arr = cpusubtypes_mc88000_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_mc88000_s) / sizeof(cpusubtypes_mc88000_s[0]));
        break;
        case CPU_TYPE_SPARC:
        cpu_subtypes_arr = cpusubtypes_sparc_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_sparc_s) / sizeof(cpusubtypes_sparc_s[0]));
        break;
        case CPU_TYPE_I860:
        cpu_subtypes_arr = cpusubtypes_i860_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_i860_s) / sizeof(cpusubtypes_i860_s[0]));
        break;
        case CPU_TYPE_POWERPC:
        case CPU_TYPE_POWERPC64:
        cpu_subtypes_arr = cpusubtypes_powerpc_s;
        cpu_subtypes_len = (sizeof(cpusubtypes_powerpc_s) / sizeof(cpusubtypes_powerpc_s[0]));
        break;
        default:
        cpu_subtypes_arr = NULL;
        cpu_subtypes_len = 0;
    }
    if (cpu_subtypes_arr != NULL && cpu_subtypes_len > 0)
        cpusubtype = get_entry_name_or_default(cpu_subtypes_arr, cpu_subtypes_len, header->cpusubtype);
    else
        cpusubtype = "<UNKNOWN>";

    // FILE TYPE
    filetype = get_entry_name_or_default(filetypes_s, (sizeof(filetypes_s) / sizeof(filetypes_s[0])), header->filetype);

    // -- FLAGS --
    headerflags = NULL;
    flaglen = 0;
    for (i = 0; i < (sizeof(headerflags_s) / sizeof(headerflags_s[0])); i++)
        if (header->flags & headerflags_s[i].value)
            flaglen = flaglen + (flaglen > 0 ? 3 : 0) + strnlen(headerflags_s[i].name, 64);

    // Force 16-byte aligment
    headerflags = calloc((flaglen + 16) & -16, 1);
    if (!headerflags) return ~1;
    ptr = headerflags;

    if (flaglen) {
        for (i = 0; i < (sizeof(headerflags_s) / sizeof(headerflags_s[0])); i++) {
            if (header->flags & headerflags_s[i].value) {
                if (ptr != headerflags)
                    ptr += snprintf(ptr, flaglen - ((size_t)(ptr - headerflags)), " | ");
                ptr += snprintf(ptr,
                    flaglen - ((size_t)(ptr - headerflags)),
                    "%s",
                    headerflags_s[i].name);
            }
        }
    } else {
        flaglen = sizeof("<UNKNOWN>") - 1;
        if (strncat(ptr, "<UNKNOWN>", flaglen + 1))
            ptr = ptr + flaglen;
    }
    if (!ptr || flaglen != ((size_t)(ptr - headerflags)))
        return ~1;

    printf(
        "%08"PRIx32"         ;            magic: %s\n"
        "%08"PRIx32"         ;          cputype: %s\n"
        "%08"PRIx32"         ;       cpusubtype: %s\n"
        "%08"PRIx32"         ;         filetype: %s\n"
        "%08"PRIx32"         ;            ncmds: %"PRIu32"\n"
        "%08"PRIx32"         ;       sizeofcmds: %"PRIu32"\n"
        "%08"PRIx32"         ;            flags: %s\n",
        to_u32be(header->magic),      magic,
        to_u32be(header->cputype),    cputype,
        to_u32be(header->cpusubtype), cpusubtype,
        to_u32be(header->filetype),   filetype,
        to_u32be(header->ncmds),      header->ncmds,
        to_u32be(header->sizeofcmds), header->sizeofcmds,
        to_u32be(header->flags),      headerflags);
    
    if (len == sizeof(struct mach_header_64))
        printf("%08"PRIx32"         ;         reserved: %"PRIu32"\n",
            to_u32be(header->reserved), header->reserved);
    
    *out = (struct mach_header*)header;
    return len;
}

/* Parses and print a single section from LC_SEGMENT_64 Command */
ssize_t parse_section_64(uint8_t *buffer, size_t len)
{
    int ret;
    char sectname_cstr[64];
    char segname_cstr[64];
    struct section_64 *sec = (struct section_64 *)buffer;
    if (len < sizeof(struct section_64)) return ~1;
    sanitize_string(sec->sectname, 16, sectname_cstr);
    sanitize_string(sec->segname, 16, segname_cstr);

    printf(
        "%016"PRIx64" ;      section name: \"%.64s\" (pad to 16 bytes)\n"
        "%016"PRIx64" ;                    \n"
        "%016"PRIx64" ;      segment name: \"%.64s\" (pad to 16 bytes)\n"
        "%016"PRIx64" ;                    \n"
        "%016"PRIx64" ;    memory address: %"PRIu64"\n"
        "%016"PRIx64" ;              size: %"PRIu64"\n"
        "%08"PRIx32"         ;    section offset: %"PRIu32"\n"
        "%08"PRIx32"         ; section alignment: %"PRIu32"\n"
        "%08"PRIx32"         ; relocation offset: %"PRIu32"\n"
        "%08"PRIx32"         ; relocation number: %"PRIu32"\n"
        "%08"PRIx32"         ;             flags: %"PRIu32"\n"
        "%08"PRIx32"         ;         reserved1: %"PRIu32"\n"
        "%08"PRIx32"         ;         reserved2: %"PRIu32"\n"
        "%08"PRIx32"         ;         reserved3: %"PRIu32"\n",
        to_u64be(*(((uint64_t*)sec->sectname)+0)), sectname_cstr,
        to_u64be(*(((uint64_t*)sec->sectname)+1)),
        to_u64be(*(((uint64_t*)sec->segname)+0)),  segname_cstr,
        to_u64be(*(((uint64_t*)sec->segname)+1)),
        to_u64be(sec->addr),                       sec->addr,
        to_u64be(sec->size),                       sec->size,
        to_u32be(sec->offset),                     sec->offset,
        to_u32be(sec->align),                      pow_u32(2, sec->align),
        to_u32be(sec->reloff),                     sec->reloff,
        to_u32be(sec->nreloc),                     sec->nreloc,
        to_u32be(sec->flags),                      sec->flags,
        to_u32be(sec->reserved1),                  sec->reserved1,
        to_u32be(sec->reserved2),                  sec->reserved2,
        to_u32be(sec->reserved3),                  sec->reserved3);
    return sizeof(struct section_64);
}

/* Parses and print LC_SEGMENT_64 Command */
ssize_t parse_segment_command_64(uint8_t *buffer, size_t len)
{
    ssize_t cmdsize, ret;
    struct segment_command_64* cmd = (struct segment_command_64*)buffer;
    if (len < sizeof(struct segment_command_64) || cmd->cmd != LC_SEGMENT_64)
        return ~1;
    cmdsize = ((ssize_t)cmd->cmdsize) - sizeof(struct segment_command_64);
    printf(
        "%08" PRIx32 "         ;          command: LC_SEGMENT_64\n"
        "%08" PRIx32 "         ;     command size: %" PRIu32 "\n"
        "%016" PRIx64 " ;     segment name: \"%.16s\" \n"
        "%016" PRIx64 " ;                   (pad to 16 bytes)\n"
        "%016" PRIx64 " ;           vmaddr: %" PRIu64 "\n"
        "%016" PRIx64 " ;           vmsize: %" PRIu64 "\n"
        "%016" PRIx64 " ;          fileoff: %" PRIu64 "\n"
        "%016" PRIx64 " ;         filesize: %" PRIu64 "\n"
        "%08" PRIx32 "         ;          maxprot: %" PRIu32 "\n"
        "%08" PRIx32 "         ;         initprot: %" PRIu32 "\n"
        "%08" PRIx32 "         ;           nsects: %" PRIu32 "\n"
        "%08" PRIx32 "         ;            flags: %" PRIu32 "\n",
        to_u32be(cmd->cmd),
        to_u32be(cmd->cmdsize), cmd->cmdsize,
        to_u64be(*(((uint64_t *)cmd->segname) + 0)), cmd->segname,
        to_u64be(*(((uint64_t *)cmd->segname) + 1)),
        to_u64be(cmd->vmaddr), cmd->vmaddr,
        to_u64be(cmd->vmsize), cmd->vmsize,
        to_u64be(cmd->fileoff), cmd->fileoff,
        to_u64be(cmd->filesize), cmd->filesize,
        to_u32be(cmd->maxprot), cmd->maxprot,
        to_u32be(cmd->initprot), cmd->initprot,
        to_u32be(cmd->nsects), cmd->nsects,
        to_u32be(cmd->flags), cmd->flags);
    
    buffer = buffer + sizeof(struct segment_command_64);
    len = len - sizeof(struct segment_command_64);
    while (cmdsize > 0) {
        printf("\n");
        ret = parse_section_64(buffer, len);
        if (ret < 0) return ret;
        cmdsize = cmdsize - ret;
        buffer = buffer + ((size_t)ret);
        len = len - ((size_t)ret);
    }
    return (ssize_t)cmd->cmdsize;
}

/*
 * Parse and print Dyld Linker Commands:
 * - LC_CODE_SIGNATURE
 * - LC_SEGMENT_SPLIT_INFO
 * - LC_FUNCTION_STARTS
 * - LC_DATA_IN_CODE
 * - LC_DYLIB_CODE_SIGN_DRS
 * - LC_LINKER_OPTIMIZATION_HINT
 * - LC_DYLD_EXPORTS_TRIE
 * - LC_DYLD_CHAINED_FIXUPS
 */
ssize_t parse_linkedit_data_command(uint8_t *buffer, size_t len)
{
    const char *cmd_name;
    struct linkedit_data_command *cmd = (struct linkedit_data_command *)buffer;
    if (len < sizeof(struct linkedit_data_command)
        || cmd->cmdsize != sizeof(struct linkedit_data_command))
        return ~1;
    cmd_name = id2lc_command_name(cmd->cmd);
    printf(
        "%08" PRIx32 "         ;          command: %.32s\n"
        "%08" PRIx32 "         ;     command size: %"PRIu32"\n"
        "%08" PRIx32 "         ;      data offset: %"PRIu32"\n"
        "%08" PRIx32 "         ;        data size: %"PRIu32"\n",
        to_u32be(cmd->cmd),      id2lc_command_name(cmd->cmd),
        to_u32be(cmd->cmdsize),  cmd->cmdsize,
        to_u32be(cmd->dataoff),  cmd->dataoff,
        to_u32be(cmd->datasize), cmd->datasize);
    return cmd->cmdsize;
}

/*
 * Parse and print LC_SYMTAB Command:
 */
ssize_t parse_symtab_command(uint8_t *buffer, size_t len)
{
    const char *cmd_name;
    struct symtab_command *cmd = (struct symtab_command *)buffer;
    if (len < sizeof(struct symtab_command) || cmd->cmdsize != sizeof(struct symtab_command) || cmd->cmd != LC_SYMTAB)
        return ~1;
    cmd_name = id2lc_command_name(cmd->cmd);
    printf(
        "%08"PRIx32"         ;          command: %.32s\n"
        "%08"PRIx32"         ;     command size: %"PRIu32"\n"
        "%08"PRIx32"         ; sym table offset: %"PRIu32"\n"
        "%08"PRIx32"         ; number of symbol: %"PRIu32"\n"
        "%08"PRIx32"         ;    string offset: %"PRIu32"\n"
        "%08"PRIx32"         ;      string size: %"PRIu32"\n",
        to_u32be(cmd->cmd),     id2lc_command_name(cmd->cmd),
        to_u32be(cmd->cmdsize), cmd->cmdsize,
        to_u32be(cmd->symoff),  cmd->symoff,
        to_u32be(cmd->nsyms),   cmd->nsyms,
        to_u32be(cmd->stroff),  cmd->stroff,
        to_u32be(cmd->strsize), cmd->strsize);
    return cmd->cmdsize;
}

/*
 * Parse and print LC_DYSYMTAB Command:
 */
ssize_t parse_dysymtab_command(uint8_t *buffer, size_t len)
{
    const char *cmd_name;
    struct dysymtab_command *cmd = (struct dysymtab_command *)buffer;
    if (len < sizeof(struct dysymtab_command)
        || cmd->cmdsize != sizeof(struct dysymtab_command) || cmd->cmd != LC_DYSYMTAB)
        return ~1;
    cmd_name = id2lc_command_name(cmd->cmd);
    printf(
        "%08"PRIx32"         ;          command: %.32s\n"
        "%08"PRIx32"         ;     command size: %"PRIu32"\n"
        "%08"PRIx32"         ; index 2 localsym: %"PRIu32"\n"
        "%08"PRIx32"         ;   number symbols: %"PRIu32"\n"
        "%08"PRIx32"         ;       iextdefsym: %"PRIu32"\n"
        "%08"PRIx32"         ;       nextdefsym: %"PRIu32"\n"
        "%08"PRIx32"         ;        iundefsym: %"PRIu32"\n"
        "%08"PRIx32"         ;        nundefsym: %"PRIu32"\n"
        "%08"PRIx32"         ;           tocoff: %"PRIu32"\n"
        "%08"PRIx32"         ;             ntoc: %"PRIu32"\n"
        "%08"PRIx32"         ;          nmodtab: %"PRIu32"\n"
        "%08"PRIx32"         ;     extrefsymoff: %"PRIu32"\n"
        "%08"PRIx32"         ;      nextrefsyms: %"PRIu32"\n"
        "%08"PRIx32"         ;   indirectsymoff: %"PRIu32"\n"
        "%08"PRIx32"         ;    nindirectsyms: %"PRIu32"\n"
        "%08"PRIx32"         ;        extreloff: %"PRIu32"\n"
        "%08"PRIx32"         ;          nextrel: %"PRIu32"\n"
        "%08"PRIx32"         ;        locreloff: %"PRIu32"\n"
        "%08"PRIx32"         ;          nlocrel: %"PRIu32"\n",
        to_u32be(cmd->cmd),            id2lc_command_name(cmd->cmd),
        to_u32be(cmd->cmdsize),        cmd->cmdsize,
        to_u32be(cmd->ilocalsym),      cmd->ilocalsym,
        to_u32be(cmd->nlocalsym),      cmd->nlocalsym,
        to_u32be(cmd->iextdefsym),     cmd->iextdefsym,
        to_u32be(cmd->nextdefsym),     cmd->nextdefsym,
        to_u32be(cmd->iundefsym),      cmd->iundefsym,
        to_u32be(cmd->nundefsym),      cmd->nundefsym,
        to_u32be(cmd->tocoff),         cmd->tocoff,
        to_u32be(cmd->ntoc),           cmd->ntoc,
        to_u32be(cmd->modtaboff),      cmd->modtaboff,
        to_u32be(cmd->nmodtab),        cmd->nmodtab,
        to_u32be(cmd->extrefsymoff),   cmd->extrefsymoff,
        to_u32be(cmd->indirectsymoff), cmd->indirectsymoff,
        to_u32be(cmd->nindirectsyms),  cmd->nindirectsyms,
        to_u32be(cmd->extreloff),      cmd->extreloff,
        to_u32be(cmd->nextrel),        cmd->nextrel,
        to_u32be(cmd->locreloff),      cmd->locreloff,
        to_u32be(cmd->nlocrel),        cmd->nlocrel);
    return cmd->cmdsize;
}

/*
 * Parse and print any LC_* command, including unknown commands.
 * TODO: format LC_UNIXTHREAD-like commands for aarch64 and x86_64.
 */
ssize_t parse_load_command(uint8_t *buffer, size_t len)
{
    size_t cmdlen;
    struct load_command *cmd;
    if (len < sizeof(struct load_command))
        return ~1;
    cmd = (struct load_command*)buffer;
    switch (cmd->cmd)
    {
        case LC_SEGMENT_64:
            return parse_segment_command_64(buffer, len);
        case LC_CODE_SIGNATURE:
        case LC_SEGMENT_SPLIT_INFO:
        case LC_FUNCTION_STARTS:
        case LC_DATA_IN_CODE:
        case LC_DYLIB_CODE_SIGN_DRS:
        case LC_LINKER_OPTIMIZATION_HINT:
        case LC_DYLD_EXPORTS_TRIE:
        case LC_DYLD_CHAINED_FIXUPS:
            return parse_linkedit_data_command(buffer, len);
        case LC_SYMTAB:
            return parse_symtab_command(buffer, len);
        case LC_DYSYMTAB:
            return parse_dysymtab_command(buffer, len);
        default:
        {
            cmdlen = cmd->cmdsize;
            printf(
                "%08" PRIx32 "         ;          command: %.32s\n"
                "%08" PRIx32 "         ;     command size: %"PRIu32"\n",
                to_u32be(cmd->cmd),      id2lc_command_name(cmd->cmd),
                to_u32be(cmd->cmdsize),  cmd->cmdsize);
            if (cmdlen < sizeof(struct load_command) || cmdlen > len) return ~1;
            buffer = buffer + sizeof(struct load_command);
            len = len - sizeof(struct load_command);
            cmdlen = cmdlen - sizeof(struct load_command);
            while (cmdlen > 0) {
                if (cmdlen >= 8) {
                    printf("%016" PRIx64 " ;\n", to_u64be(*((uint64_t*)buffer)));
                    buffer = buffer + 8;
                    len = len - 8;
                    cmdlen = cmdlen - 8;
                } else if (cmdlen >= 4) {
                    printf("%08"PRIx32"         ;\n", to_u32be(*((uint32_t*)buffer)));
                    buffer = buffer + 4;
                    len = len - 4;
                    cmdlen = cmdlen - 4;
                } else if (cmdlen >= 2) {
                    printf("%04"PRIx16"                 ;\n", to_u16be(*((uint8_t*)buffer)));
                    buffer = buffer + 2;
                    len = len - 2;
                    cmdlen = cmdlen - 2;
                } else if (cmdlen == 1) {
                    printf("%02"PRIx8"                   ;\n", *buffer);
                    buffer = buffer + 1;
                    len = len - 1;
                    cmdlen = cmdlen - 1;
                }
            }
            return (ssize_t)cmd->cmdsize;
        }
    }
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

    // Parse Mach-O Header
    ret = parse_mach_header(buffer, len, &header);
    if (ret <= 0 || header == NULL || ((size_t)ret) > len) return ((int)~ret) | ((int)(ret == 0));
    len = len - ret;
    buffer = buffer + ret;

    // Parse Load Commands
    cmd_count = cmd_size = 0;
    while (cmd_count < header->ncmds)
    {
        cmd_count = cmd_count + 1;
        printf("\n                 ; -- LOAD COMMAND %"PRIu32" --\n", cmd_count);
        ret = parse_load_command(buffer, len);

        // Failed to parse load command
        if (ret <= 0)
            fail(1, "[ERROR] failed to parse command %"PRIu32"\n", cmd_count);

        // Command size must be within header limits.
        if (ret > (header->sizeofcmds - cmd_size))
            fail(1,
                "[ERROR] command %"PRIu32"/%"PRIu32" size out of bounds.\n"
                "        command size: %"PRId64"\n"
                "        maximum size: %"PRIu32"\n",
                cmd_count, header->ncmds, (int64_t)ret, (header->sizeofcmds - cmd_size));

        len = len - ret;
        buffer = buffer + ret;
        cmd_size = cmd_size + ret;
    }

    // Handles the case where the total command size is less than expected
    if (cmd_size != header->sizeofcmds)
        fail(1,
            "[ERROR] total commands size mismatch"
            "        (1) mach-o commands size is: %"PRIu32"\n"
            "        (2) found %"PRIu32" commands with total size of %"PRIu32"\n",
            header->sizeofcmds,
            cmd_count,
            cmd_size);

    return ret;
}

/* Mach2Hex0 main */
int main(int argc, char *argv[]) {
    char *cmd = "";
    FILE *file = NULL;
    uint8_t buffer[4096];
    size_t len;
    char actualpath[PATH_MAX + 1];
    struct stat st;

    /* Validate arguments */
    if (argc && argv && argv[0])
        cmd = argv[0];

    /* TODO: Replace this by the USAGE message */
    if (argc < 2 || !argv || !argv[1])
        fail(1, "[ERROR] %s: missing operand\n", cmd);
    if (argc > 3)
        fail(1, "[ERROR] %s: extra operand '%s'\n", cmd, argv[2]);
    if (stat(argv[1], &st) != 0)
        fail(1, "[ERROR] %s: file not found '%s'\n", cmd, argv[1]);
    if (!S_ISREG(st.st_mode))
        fail(1, "[ERROR] %s: not a file '%s'\n", cmd, argv[1]);
    if (st.st_size < sizeof(buffer))
        fail(1, "[ERROR] %s: less than 4096 bytes in size '%s'\n", cmd, argv[1]);
    if ((file = fopen(argv[1], "rb")) == NULL)
        fail(1, "[ERROR] %s: cannot open file '%s'\n", cmd, argv[1]);

    /* print the filename and size in bytes */
    printf(
        "                 ; FILE NAME: '%s'\n"
        "                 ; FILE SIZE: %"PRIu64" bytes\n"
        "\n",
        argv[1], (uint64_t)st.st_size);

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
