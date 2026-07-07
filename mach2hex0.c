/*
 * Copyright (C) 2026 Lohann Paterno Coutinho Ferreira (developer@lohann.dev)
 * 
 * Mach2Hex0 is a helper programn for converting an mach-o executable to Hex0 file.
 * This tool is system agnostic, must work in any system with a minimal C Compiler and libc.
 * 
 * Mach-O Reference:
 * - https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/EXTERNAL_HEADERS/mach-o/loader.h#L50-L81
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
#include "mach2hex0.h"

/* Check if the C compiler supports '__builtin_bswap*' family builtins */
#if defined(__has_builtin)
#define hex0_has_builtin(builtin) __has_builtin(__builtin_##builtin)
#else
#define hex0_has_builtin(builtin) 0
#endif

#if defined(__has_attribute)
#define hex0_has_attribute(attribute) __has_attribute(attribute)
#else
#define hex0_has_attribute(attribute) 0
#endif

/* Swap uint16_t endianess */
#if hex0_has_builtin(bswap16) || defined(__GNUC__)
#define hex0_bswap16(val) __builtin_bswap16((uint16_t)val)
#else
static inline uint16_t hex0_bswap16(uint16_t val) {
    return (uint16_t)((val & UINT16_C(0xFF00)) >> 8) |
           (uint16_t)((val & UINT16_C(0x00FF)) << 8);
}
#endif

/* Swap uint32_t endianess */
#if hex0_has_builtin(bswap32) || defined(__GNUC__)
#define hex0_bswap32(val) __builtin_bswap32((uint32_t)val)
#else
static inline uint32_t hex0_bswap32(uint32_t val) {
    return (uint32_t)hex0_bswap16((uint16_t)((val & UINT32_C(0x0000FFFF)) >>  0)) << 16 |
           (uint32_t)hex0_bswap16((uint16_t)((val & UINT32_C(0xFFFF0000)) >> 16)) >>  0;
}
#endif

/* Swap uint64_t endianess */
#if hex0_has_builtin(bswap64) || defined(__GNUC__)
#define hex0_bswap64(val) __builtin_bswap64((uint64_t)val)
#else
static inline uint64_t hex0_bswap64(uint64_t val) {
    return (uint64_t)hex0_bswap32((uint32_t)((val & UINT64_C(0x00000000FFFFFFFF)) >>  0)) << 32 |
           (uint64_t)hex0_bswap32((uint32_t)((val & UINT64_C(0xFFFFFFFF00000000)) >> 32)) >>  0;
}
#endif

/* Convert values between little-endian and big-endian */
#if defined(_MSC_VER) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define hex0_u16be(val) hex0_bswap16(val)
#define hex0_u32be(val) hex0_bswap32(val)
#define hex0_u64be(val) hex0_bswap64(val)
#define hex0_u16le(val) val
#define hex0_u32le(val) val
#define hex0_u64le(val) val
#else
#define hex0_u16be(val) val
#define hex0_u32be(val) val
#define hex0_u64be(val) val
#define hex0_u16le(val) hex0_bswap16(val)
#define hex0_u32le(val) hex0_bswap32(val)
#define hex0_u64le(val) hex0_bswap64(val)
#endif

/* Store the string representation of Mach-O Constants */
typedef struct macho_entry_t {
    uint32_t flag;
    const char *name;
    const char *desc;
} macho_entry_t;

/*
 * String representation and description of Mach-O Constants.
 */
#define entry(n,d) ((macho_entry_t){ .flag = hex0_u32le((uint32_t)(n)), .name = #n, .desc = d })

/* First 4 bytes of any Mach-o file */
static macho_entry_t mach_magics_s[4] = {
    entry(MH_MAGIC,                    "The mach magic number (32-bit little-endian)"),
    entry(MH_MAGIC_64,                 "The mach magic number (64-bit little-endian)"),
    entry(MH_CIGAM,                    "The mach magic number (32-bit big-endian)"),
    entry(MH_CIGAM_64,                 "The mach magic number (64-bit big-endian)")
};

/* All known mach-o Load Commands */
static macho_entry_t load_commands_s[54] = {
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
    entry(LC_LOAD_WEAK_DYLIB,          "load a dynamically linked shared library that is allowed to be missing (all symbols are weak imported)."),
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

/* Mach-o header CPU types */
static macho_entry_t cputypes_s[16] = {
    entry(CPU_TYPE_POWERPC64, NULL),
    entry(CPU_TYPE_POWERPC,   NULL),
    entry(CPU_TYPE_I860,      NULL),
    entry(CPU_TYPE_SPARC,     NULL),
    entry(CPU_TYPE_MC88000,   NULL),
    entry(CPU_TYPE_ARM64_32,  NULL),
    entry(CPU_TYPE_ARM64,     NULL),
    entry(CPU_TYPE_ARM,       NULL),
    entry(CPU_TYPE_HPPA,      NULL),
    entry(CPU_TYPE_MC98000,   NULL),
    entry(CPU_TYPE_X86_64,    NULL),
    entry(CPU_TYPE_I386,      NULL),
    entry(CPU_TYPE_X86,       NULL),
    entry(CPU_TYPE_MC680x0,   NULL),
    entry(CPU_TYPE_VAX,       NULL),
    entry(CPU_TYPE_ANY,       NULL)
};

/* ARM cpu subtypes */
static macho_entry_t cpusubtypes_s[17] = {
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

/* ARM64 cpu subtypes */
static macho_entry_t cpusubtypes_arm64_s[3] = {
    entry(CPU_SUBTYPE_ARM64E,    NULL),
    entry(CPU_SUBTYPE_ARM64_V8,  NULL),
    entry(CPU_SUBTYPE_ARM64_ALL, NULL)
};

/* ARM64_32 cpu subtypes */
static macho_entry_t cpusubtypes_arm64_32_s[2] = {
    entry(CPU_SUBTYPE_ARM64_32_V8,  NULL),
    entry(CPU_SUBTYPE_ARM64_32_ALL, NULL)
};

/* I386 cpu subtypes */
static macho_entry_t cpusubtypes_i386_s[22] = {
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

/* X86 cpu subtypes. */
static macho_entry_t cpusubtypes_x86_s[4] = {
    entry(CPU_SUBTYPE_X86_64_H,   "Haswell feature subset"),
    entry(CPU_SUBTYPE_X86_ARCH1,  NULL),
    entry(CPU_SUBTYPE_X86_ALL,    NULL),
    entry(CPU_SUBTYPE_X86_64_ALL, NULL)
};

/* Mach-o header file type, ex:
 * - MH_EXECUTE for executable files
 * = MH_DYLIB for *.dylib files */
static macho_entry_t filetypes_s[11] = {
    { .flag = MH_OBJECT,
      .name = "MH_OBJECT",
      .desc = "relocatable object file" },
    { .flag = MH_EXECUTE,
      .name = "MH_EXECUTE",
      .desc = "demand paged executable file" },
    { .flag = MH_FVMLIB,
      .name = "MH_FVMLIB",
      .desc = "fixed VM shared library file" },
    { .flag = MH_CORE,
      .name = "MH_CORE",
      .desc = "core file" },
    { .flag = MH_PRELOAD,
      .name = "MH_PRELOAD",
      .desc = "preloaded executable file" },
    { .flag = MH_DYLIB,
      .name = "MH_DYLIB",
      .desc = "dynamically bound shared library" },
    { .flag = MH_DYLINKER,
      .name = "MH_DYLINKER",
      .desc = "dynamic link editor" },
    { .flag = MH_BUNDLE,
      .name = "MH_BUNDLE",
      .desc = "dynamically bound bundle file" },
    { .flag = MH_DYLIB_STUB,
      .name = "MH_DYLIB_STUB",
      .desc = "shared library stub for static linking only, no section contents" },
    { .flag = MH_DSYM,
      .name = "MH_DSYM",
      .desc = "companion file with only debug sections" },
    { .flag = MH_KEXT_BUNDLE,
      .name = "MH_KEXT_BUNDLE",
      .desc = "x86_64 kexts" }
};

/* Bitflags used in Mach-o Header */
static macho_entry_t headerflags_s[29] = {
    { .flag = MH_NOUNDEFS,
      .name = "MH_NOUNDEFS",
      .desc = "the object file has no undefined references" },
    { .flag = MH_INCRLINK,
      .name = "MH_INCRLINK",
      .desc = "the object file is the output of an incremental link against a base file and can't be link edited again" },
    { .flag = MH_DYLDLINK,
      .name = "MH_DYLDLINK",
      .desc = "the object file is input for the dynamic linker and can't be statically link edited again" },
    { .flag = MH_BINDATLOAD,
      .name = "MH_BINDATLOAD",
      .desc = "the object file's undefined references are bound by the dynamic linker when loaded." },
    { .flag = MH_PREBOUND,
      .name = "MH_PREBOUND",
      .desc = "the file has its dynamic undefined references prebound." },
    { .flag = MH_SPLIT_SEGS,
      .name = "MH_SPLIT_SEGS",
      .desc = "the file has its read-only and read-write segments split" },
    { .flag = MH_LAZY_INIT,
      .name = "MH_LAZY_INIT",
      .desc = "the shared library init routine is to be run lazily via catching memory faults to its writeable segments (obsolete)" },
    { .flag = MH_TWOLEVEL,
      .name = "MH_TWOLEVEL",
      .desc = "the image is using two-level name space bindings" },
    { .flag = MH_FORCE_FLAT,
      .name = "MH_FORCE_FLAT",
      .desc = "the executable is forcing all images to use flat name space bindings" },
    { .flag = MH_NOMULTIDEFS,
      .name = "MH_NOMULTIDEFS",
      .desc = "this umbrella guarantees no multiple definitions of symbols in its sub-images so the two-level namespace hints can always be used." },
    { .flag = MH_NOFIXPREBINDING,
      .name = "MH_NOFIXPREBINDING",
      .desc = "do not have dyld notify the prebinding agent about this executable" },
    { .flag = MH_PREBINDABLE,
      .name = "MH_PREBINDABLE",
      .desc = "the binary is not prebound but can have its prebinding redone. only used when MH_PREBOUND is not set." },
    { .flag = MH_ALLMODSBOUND,
      .name = "MH_ALLMODSBOUND",
      .desc = "indicates that this binary binds to all two-level namespace modules of its dependent libraries. only used when MH_PREBINDABLE and MH_TWOLEVEL are both set." },
    { .flag = MH_SUBSECTIONS_VIA_SYMBOLS,
      .name = "MH_SUBSECTIONS_VIA_SYMBOLS",
      .desc = "safe to divide up the sections into sub-sections via symbols for dead code stripping" },
    { .flag = MH_CANONICAL,
      .name = "MH_CANONICAL",
      .desc = "the binary has been canonicalized via the unprebind operation" },
    { .flag = MH_WEAK_DEFINES,
      .name = "MH_WEAK_DEFINES",
      .desc = "the final linked image contains external weak symbols" },
    { .flag = MH_BINDS_TO_WEAK,
      .name = "MH_BINDS_TO_WEAK",
      .desc = "the final linked image uses weak symbols" },
    { .flag = MH_ALLOW_STACK_EXECUTION,
      .name = "MH_ALLOW_STACK_EXECUTION",
      .desc = "When this bit is set, all stacks in the task will be given stack execution privilege. Only used in MH_EXECUTE filetypes." },
    { .flag = MH_ROOT_SAFE,
      .name = "MH_ROOT_SAFE",
      .desc = "When this bit is set, the binary declares it is safe for use in processes with uid zero" },
    { .flag = MH_SETUID_SAFE,
      .name = "MH_SETUID_SAFE",
      .desc = "When this bit is set, the binary declares it is safe for use in processes when issetugid() is true" },
    { .flag = MH_NO_REEXPORTED_DYLIBS,
      .name = "MH_NO_REEXPORTED_DYLIBS",
      .desc = "When this bit is set on a dylib, the static linker does not need to examine dependent dylibs to see if any are re-exported" },
    { .flag = MH_PIE,
      .name = "MH_PIE",
      .desc = "When this bit is set, the OS will load the main executable at a random address. Only used in MH_EXECUTE filetypes." },
    { .flag = MH_DEAD_STRIPPABLE_DYLIB,
      .name = "MH_DEAD_STRIPPABLE_DYLIB",
      .desc = "Only for use on dylibs.  When linking against a dylib that has this bit set, the static linker will automatically not create a LC_LOAD_DYLIB load command to the dylib if no symbols are being referenced from the dylib." },
    { .flag = MH_HAS_TLV_DESCRIPTORS,
      .name = "MH_HAS_TLV_DESCRIPTORS",
      .desc = "Contains a section of type S_THREAD_LOCAL_VARIABLES" },
    { .flag = MH_NO_HEAP_EXECUTION,
      .name = "MH_NO_HEAP_EXECUTION",
      .desc = "When this bit is set, the OS will run the main executable with a non-executable heap even on platforms (e.g. x86) that don't require it. Only used in MH_EXECUTE filetypes." },
    { .flag = MH_APP_EXTENSION_SAFE,
      .name = "MH_APP_EXTENSION_SAFE",
      .desc = "The code was linked for use in an application extension." },
    { .flag = MH_NLIST_OUTOFSYNC_WITH_DYLDINFO,
      .name = "MH_NLIST_OUTOFSYNC_WITH_DYLDINFO",
      .desc = "The external symbols listed in the nlist symbol table do not include all the symbols listed in the dyld info." },
    { .flag = MH_SIM_SUPPORT,
      .name = "MH_SIM_SUPPORT",
      .desc = "Allow LC_MIN_VERSION_MACOS and LC_BUILD_VERSION load commands with the platforms macOS, iOSMac, iOSSimulator, tvOSSimulator and watchOSSimulator." },
    { .flag = MH_DYLIB_IN_CACHE,
      .name = "MH_DYLIB_IN_CACHE (x86_64 kexts)",
      .desc = "Only for use on dylibs. When this bit is set, the dylib is part of the dyld shared cache, rather than loose in the filesystem." }
};
#undef entry // we no longer need this macro.

/* Detect if the C compiler supports noreturn attribute */
__attribute__((__noreturn__))
#if __STDC_VERSION__ >= 202311L
void fail(int code, const char* err) [[noreturn]];
#elif __STDC_VERSION__ >= 201112L
void fail(int code, const char* err) _Noreturn;
#elif hex0_has_attribute(noreturn) || defined(__GNUC__) || defined(__TINYC__)
void fail(int code, const char* err) __attribute__((noreturn));
#elif defined(zig_msvc)
void fail(int code, const char* err) __declspec(noreturn);
#endif

/* Fail and exit */
void fail(int code, const char* err)
{
    fprintf(stderr, "[ERROR] %s\n", err);
    exit((int)code);
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

/* Retrieves the Load Command Name if the ID is known, otherwise "<UNKNOWN>" */
static const char* id2lc_command_name(uint32_t id)
{
    int i = 0;
    for (i = 0; i < (sizeof(load_commands_s) / sizeof(load_commands_s[0])); i++)
        if (load_commands_s[i].flag == id)
            return load_commands_s[i].name;
    return "<UNKNOWN>";
}

/* Parses the Mach-o Header
 * TODO: add support to 32-bit headers */
ssize_t parse_mach_header(uint8_t *buffer, size_t len, struct mach_header_64** out)
{
    int i;
    size_t flaglen;
    struct mach_header_64* header;
    const char *magic, *cputype, *cpusubtype, *filetype;
    char *headerflags, *ptr;
    if (!buffer || !out || len < sizeof(struct mach_header_64))
        return ~1;
    header = (struct mach_header_64*) buffer;
    magic = cputype = cpusubtype = filetype = "<UNKNOWN>";

    // MAGIC
    for (i = 0; i < (sizeof(mach_magics_s) / sizeof(mach_magics_s[0])); i++) {
        if (header->magic == mach_magics_s[i].flag) {
            magic = mach_magics_s[i].name;
            break;
        }
    }

    // CPU TYPE
    for (i = 0; i < (sizeof(cputypes_s) / sizeof(cputypes_s[0])); i++) {
        if (header->cputype == cputypes_s[i].flag) {
            cputype = cputypes_s[i].name;
            break;
        }
    }

    // CPU TYPE
    for (i = 0; i < (sizeof(cpusubtypes_s) / sizeof(cpusubtypes_s[0])); i++) {
        if (header->cpusubtype == cpusubtypes_s[i].flag) {
            cpusubtype = cpusubtypes_s[i].name;
            break;
        }
    }

    // FILE TYPE
    for (i = 0; i < (sizeof(filetypes_s) / sizeof(filetypes_s[0])); i++) {
        if (header->filetype == filetypes_s[i].flag) {
            filetype = filetypes_s[i].name;
            break;
        }
    }

    // -- FLAGS --
    headerflags = NULL;
    flaglen = 0;
    for (i = 0; i < (sizeof(headerflags_s) / sizeof(headerflags_s[0])); i++)
        if (header->flags & headerflags_s[i].flag)
            flaglen = flaglen + (flaglen > 0 ? 3 : 0) + strnlen(headerflags_s[i].name, 64);

    // Force 16-byte aligment
    headerflags = calloc((flaglen + 16) & -16, 1);
    if (!headerflags) return ~1;
    ptr = headerflags;

    if (flaglen) {
        for (i = 0; i < (sizeof(headerflags_s) / sizeof(headerflags_s[0])); i++) {
            if (header->flags & headerflags_s[i].flag) {
                if (ptr != headerflags)
                    ptr += snprintf(ptr, flaglen - ((size_t)(ptr - headerflags)), " | ");
                ptr += snprintf(ptr, flaglen - ((size_t)(ptr - headerflags)), "%s", headerflags_s[i].name);
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
        "%08"PRIx32"         ;            flags: %s\n"
        "%08"PRIx32"         ;         reserved: %"PRIu32"\n",
        hex0_u32be(header->magic),      magic,
        hex0_u32be(header->cputype),    cputype,
        hex0_u32be(header->cpusubtype), cpusubtype,
        hex0_u32be(header->filetype),   filetype,
        hex0_u32be(header->ncmds),      header->ncmds,
        hex0_u32be(header->sizeofcmds), header->sizeofcmds,
        hex0_u32be(header->flags),      headerflags,
        hex0_u32be(header->reserved),   header->reserved);
    *out = header;
    return sizeof(struct mach_header_64);
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
        hex0_u64be(*(((uint64_t*)sec->sectname)+0)), sectname_cstr,
        hex0_u64be(*(((uint64_t*)sec->sectname)+1)),
        hex0_u64be(*(((uint64_t*)sec->segname)+0)),  segname_cstr,
        hex0_u64be(*(((uint64_t*)sec->segname)+1)),
        hex0_u64be(sec->addr),                       sec->addr,
        hex0_u64be(sec->size),                       sec->size,
        hex0_u32be(sec->offset),                     sec->offset,
        hex0_u32be(sec->align),                      pow_u32(2, sec->align),
        hex0_u32be(sec->reloff),                     sec->reloff,
        hex0_u32be(sec->nreloc),                     sec->nreloc,
        hex0_u32be(sec->flags),                      sec->flags,
        hex0_u32be(sec->reserved1),                  sec->reserved1,
        hex0_u32be(sec->reserved2),                  sec->reserved2,
        hex0_u32be(sec->reserved3),                  sec->reserved3);
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
        hex0_u32be(cmd->cmd),
        hex0_u32be(cmd->cmdsize), cmd->cmdsize,
        hex0_u64be(*(((uint64_t *)cmd->segname) + 0)), cmd->segname,
        hex0_u64be(*(((uint64_t *)cmd->segname) + 1)),
        hex0_u64be(cmd->vmaddr), cmd->vmaddr,
        hex0_u64be(cmd->vmsize), cmd->vmsize,
        hex0_u64be(cmd->fileoff), cmd->fileoff,
        hex0_u64be(cmd->filesize), cmd->filesize,
        hex0_u32be(cmd->maxprot), cmd->maxprot,
        hex0_u32be(cmd->initprot), cmd->initprot,
        hex0_u32be(cmd->nsects), cmd->nsects,
        hex0_u32be(cmd->flags), cmd->flags);
    
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
    if (len < sizeof(struct linkedit_data_command) || cmd->cmdsize != sizeof(struct linkedit_data_command))
        return ~1;
    cmd_name = id2lc_command_name(cmd->cmd);
    printf(
        "%08" PRIx32 "         ;          command: %.32s\n"
        "%08" PRIx32 "         ;     command size: %"PRIu32"\n"
        "%08" PRIx32 "         ;      data offset: %"PRIu32"\n"
        "%08" PRIx32 "         ;        data size: %"PRIu32"\n",
        hex0_u32be(cmd->cmd),      id2lc_command_name(cmd->cmd),
        hex0_u32be(cmd->cmdsize),  cmd->cmdsize,
        hex0_u32be(cmd->dataoff),  cmd->dataoff,
        hex0_u32be(cmd->datasize), cmd->datasize);
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
        hex0_u32be(cmd->cmd),     id2lc_command_name(cmd->cmd),
        hex0_u32be(cmd->cmdsize), cmd->cmdsize,
        hex0_u32be(cmd->symoff),  cmd->symoff,
        hex0_u32be(cmd->nsyms),   cmd->nsyms,
        hex0_u32be(cmd->stroff),  cmd->stroff,
        hex0_u32be(cmd->strsize), cmd->strsize);
    return cmd->cmdsize;
}

/*
 * Parse and print LC_DYSYMTAB Command:
 */
ssize_t parse_dysymtab_command(uint8_t *buffer, size_t len)
{
    const char *cmd_name;
    struct dysymtab_command *cmd = (struct dysymtab_command *)buffer;
    if (len < sizeof(struct dysymtab_command) || cmd->cmdsize != sizeof(struct dysymtab_command) || cmd->cmd != LC_DYSYMTAB)
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
        hex0_u32be(cmd->cmd),         id2lc_command_name(cmd->cmd),
        hex0_u32be(cmd->cmdsize),        cmd->cmdsize,
        hex0_u32be(cmd->ilocalsym),      cmd->ilocalsym,
        hex0_u32be(cmd->nlocalsym),      cmd->nlocalsym,
        hex0_u32be(cmd->iextdefsym),     cmd->iextdefsym,
        hex0_u32be(cmd->nextdefsym),     cmd->nextdefsym,
        hex0_u32be(cmd->iundefsym),      cmd->iundefsym,
        hex0_u32be(cmd->nundefsym),      cmd->nundefsym,
        hex0_u32be(cmd->tocoff),         cmd->tocoff,
        hex0_u32be(cmd->ntoc),           cmd->ntoc,
        hex0_u32be(cmd->modtaboff),      cmd->modtaboff,
        hex0_u32be(cmd->nmodtab),        cmd->nmodtab,
        hex0_u32be(cmd->extrefsymoff),   cmd->extrefsymoff,
        hex0_u32be(cmd->indirectsymoff), cmd->indirectsymoff,
        hex0_u32be(cmd->nindirectsyms),  cmd->nindirectsyms,
        hex0_u32be(cmd->extreloff),      cmd->extreloff,
        hex0_u32be(cmd->nextrel),        cmd->nextrel,
        hex0_u32be(cmd->locreloff),      cmd->locreloff,
        hex0_u32be(cmd->nlocrel),        cmd->nlocrel);
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
                hex0_u32be(cmd->cmd),      id2lc_command_name(cmd->cmd),
                hex0_u32be(cmd->cmdsize),  cmd->cmdsize);
            if (cmdlen < sizeof(struct load_command) || cmdlen > len) return ~1;
            buffer = buffer + sizeof(struct load_command);
            len = len - sizeof(struct load_command);
            cmdlen = cmdlen - sizeof(struct load_command);
            while (cmdlen > 0)
            {
                if (cmdlen >= 8) {
                    printf("%016" PRIx64 " ;\n", hex0_u64be(*((uint64_t*)buffer)));
                    buffer = buffer + 8;
                    len = len - 8;
                    cmdlen = cmdlen - 8;
                    continue;
                }
                if (cmdlen >= 4) {
                    printf("%08"PRIx32"         ;\n", hex0_u32be(*((uint32_t*)buffer)));
                    buffer = buffer + 4;
                    len = len - 4;
                    cmdlen = cmdlen - 4;
                    continue;
                }
                if (cmdlen >= 2) {
                    printf("%04"PRIx16"                 ;\n", hex0_u16be(*((uint8_t*)buffer)));
                    buffer = buffer + 2;
                    len = len - 2;
                    cmdlen = cmdlen - 2;
                }
                if (cmdlen == 1) {
                    printf("%02"PRIx8"                   ;\n", *buffer);
                    buffer = buffer + 1;
                    len = len - 1;
                    cmdlen = cmdlen - 1;
                    break;
                }
            }
            return (ssize_t)cmd->cmdsize;
        }
    }
}

/*
 * Mach-o parser entrypoint.
 *
 * - buffer: raw mach-o file bytes
 * - len: mach-o file length in bytes
 */
int parse(uint8_t *buffer, size_t len)
{
    uint32_t cmd_count, cmd_size;
    ssize_t ret;
    struct mach_header_64* header = NULL;

    // Parse Mach-o Header
    ret = parse_mach_header(buffer, len, &header);
    if (ret <= 0 || header == NULL || ((size_t)ret) > len) return ((int)~ret) | ((int)(ret == 0));
    len = len - ret;
    buffer = buffer + ret;

    // Parse Mach-o Commands
    cmd_count = cmd_size = 0;
    while (cmd_count < header->ncmds)
    {
        cmd_count = cmd_count + 1;
        printf("\n                 ; -- LOAD COMMAND %"PRIu32" --\n", cmd_count);
        ret = parse_load_command(buffer, len);

        // Failed to parse load command
        if (ret <= 0) {
            fprintf(
            stderr,
            "[ERROR] failed to parse command %"PRIu32"\n", cmd_count);
            return ~1;
        }

        // Command size must be within header limits.
        if (ret > (header->sizeofcmds - cmd_size)) {
            fprintf(
                stderr,
                "[ERROR] command %"PRIu32"/%"PRIu32" size out of bounds.\n"
                "        command size: %"PRId64"\n"
                "        maximum size: %"PRIu32"\n",
                cmd_count, header->ncmds, (int64_t)ret, (header->sizeofcmds - cmd_size));
            return ~1;
        }

        len = len - ret;
        buffer = buffer + ret;
        cmd_size = cmd_size + ret;
    }

    // Handles the case where the total command size is less than expected
    if (cmd_size != header->sizeofcmds) {
        fprintf(
            stderr,
            "[ERROR] total commands size mismatch"
            "        (1) mach-o commands size is: %"PRIu32"\n"
            "        (2) found %"PRIu32" commands with total size of %"PRIu32"\n", header->sizeofcmds, cmd_count, cmd_size);
        return ~1;
    }

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

    if (argc < 2 || !argv || !argv[1]) {
        /* TODO: Replace this by the USAGE message */
        fprintf(stderr, "[ERROR] %s: missing operand\n", cmd);
        return 1;
    }
    if (argc > 3) {
        fprintf(stderr, "[ERROR] %s: extra operand '%s'\n", cmd, argv[2]);
        return 1;
    }
    if (stat(argv[1], &st) != 0) {
        fprintf(stderr, "[ERROR] %s: file not found '%s'\n", cmd, argv[1]);
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "[ERROR] %s: not a file '%s'\n", cmd, argv[1]);
        return 1;
    }
    if (st.st_size < sizeof(buffer))
    {
        fprintf(stderr, "[ERROR] %s: less than 4096 bytes in size '%s'\n", cmd, argv[1]);
        return 1;
    }
    if ((file = fopen(argv[1], "rb")) == NULL)
    {
        fprintf(stderr, "[ERROR] %s: cannot open file '%s'\n", cmd, argv[1]);
        return 1;
    }

    /* print the filename and size in bytes */
    printf(
        "                 ; FILE NAME: '%s'\n"
        "                 ; FILE SIZE: %"PRIu64" bytes\n"
        "\n",
        argv[1], (uint64_t)st.st_size);

    /* read the first 4096 bytes from file (which is mach-o minimum size) */
    len = fread(buffer, 1, sizeof(buffer) - 1, file);
    if (fclose(file) != 0) {
        fprintf(stderr, "[ERROR] %s: cannot close file '%s'\n", cmd, argv[1]);
        return 1;
    }
    if (len == 0 || len >= sizeof(buffer)) {
        snprintf((char*)buffer, sizeof(buffer) - 1, "[ERROR] %s: failed to read file '%s'\n", cmd, argv[1]);
        fail(1, (const char*)buffer);
        return 1;
    }

    /* Make sure the buffer ends with zero */
    buffer[len] = 0;

    /* Flush any pending print */
    fflush(stdin);

    /* start parser */
    return parse(buffer, len);;
}
