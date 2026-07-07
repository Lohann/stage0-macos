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

/*
 * This file describes the format of mach object files.
 */
#include <stdint.h>

typedef int32_t cpu_type_t;
typedef int32_t cpu_subtype_t;

/*
 * The 32-bit mach header appears at the very beginning of the object file for
 * 32-bit architectures.
 */
struct mach_header {
    uint32_t magic;          /* mach magic number identifier */
    cpu_type_t cputype;      /* cpu specifier */
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
// struct __attribute__((packed)) vm_prot_t {
//     READ: uint32_t = false,
//     WRITE: uint32_t = false,
//     EXEC: uint32_t = false,
//     _: u1 = 0,
//     /// When a caller finds that they cannot obtain write permission on a
//     /// mapped entry, the following flag can be used. The entry will be
//     /// made "needs copy" effectively copying the object (using COW),
//     /// and write permission will be added to the maximum protections for
//     /// the associated entry.
//     COPY: uint32_t = false,
//     __: u27 = 0,
// };

// /// The segment load command indicates that a part of this file is to be
// /// mapped into the task's address space.  The size of this segment in memory,
// /// vmsize, maybe equal to or larger than the amount to map from this file,
// /// filesize.  The file is mapped starting at fileoff to the beginning of
// /// the segment in memory, vmaddr.  The rest of the memory of the segment,
// /// if any, is allocated zero fill on demand.  The segment's maximum virtual
// /// memory protection and initial virtual memory protection are specified
// /// by the maxprot and initprot fields.  If the segment has sections then the
// /// section structures directly follow the segment command and their size is
// /// reflected in cmdsize.
// #define segment_command extern struct {
//     /// LC_SEGMENT
//     cmd: LC .SEGMENT,

//     /// includes sizeof section structs
//     cmdsize;

//     /// segment name
//     segname: [16]u8,

//     /// memory address of this segment
//     vmaddr;

//     /// memory size of this segment
//     vmsize;

//     /// file offset of this segment
//     fileoff;

//     /// amount to map from the file
//     filesize;

//     /// maximum VM protection
//     maxprot: vm_prot_t,

//     /// initial VM protection
//     initprot: vm_prot_t,

//     /// number of sections in segment
//     nsects;
//     flags;
// }

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
struct segment_command { // for 32-bit architectures */
	uint32_t cmd;		/* LC_SEGMENT */
	uint32_t cmdsize;	/* includes sizeof section structs */
	char segname[16];	/* segment name */
	uint32_t vmaddr;		/* memory address of this segment */
	uint32_t vmsize;		/* memory size of this segment */
	uint32_t fileoff;	/* file offset of this segment */
	uint32_t filesize;	/* amount to map from the file */
	vm_prot_t maxprot;	/* maximum VM protection */
	vm_prot_t initprot;	/* initial VM protection */
	uint32_t nsects;		/* number of sections in segment */
	uint32_t flags;		/* flags */
};

// #define PROT struct {
//     /// [MC2] no permissions
//     #define NONE: vm_prot_t 0x00
//     /// [MC2] pages can be read
//     #define READ: vm_prot_t 0x01
//     /// [MC2] pages can be written
//     #define WRITE: vm_prot_t 0x02
//     /// [MC2] pages can be executed
//     #define EXEC: vm_prot_t 0x04
//     /// When a caller finds that they cannot obtain write permission on a
//     /// mapped entry, the following flag can be used. The entry will be
//     /// made "needs copy" effectively copying the object (using COW),
//     /// and write permission will be added to the maximum protections for
//     /// the associated entry.
//     #define COPY: vm_prot_t 0x10
// }

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

// fn parseName(name: *const [16]u8) []const u8 {
//     const len mem.findScalar(u8, name, @as(u8, 0)) orelse name.len
//     return name[0..len]
// }

struct nlist {
    uint32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    int16_t n_desc;
    uint32_t n_value;
};

// struct nlist_64 {
//     uint32_t n_strx;
//     n_type: union(u8) /* packed */ {
//         bits: packed struct(u8) {
//             ext: bool,
//             type: enum(u3) {
//                 undf 0,
//                 abs 1,
//                 sect 7,
//                 pbud 6,
//                 indr 5,
//                 _,
//             },
//             pext: bool,
//             /// Any non-zero value indicates this is an stab, so the `stab` field should be used.
//             is_stab: u3,
//         },
//         stab: enum(u8) {
//             gsym N_GSYM,
//             fname N_FNAME,
//             fun N_FUN,
//             stsym N_STSYM,
//             lcsym N_LCSYM,
//             bnsym N_BNSYM,
//             ast N_AST,
//             opt N_OPT,
//             rsym N_RSYM,
//             sline N_SLINE,
//             ensym N_ENSYM,
//             ssym N_SSYM,
//             so N_SO,
//             oso N_OSO,
//             lsym N_LSYM,
//             bincl N_BINCL,
//             sol N_SOL,
//             params N_PARAMS,
//             version N_VERSION,
//             olevel N_OLEVEL,
//             psym N_PSYM,
//             eincl N_EINCL,
//             entry N_ENTRY,
//             lbrac N_LBRAC,
//             excl N_EXCL,
//             rbrac N_RBRAC,
//             bcomm N_BCOMM,
//             ecomm N_ECOMM,
//             ecoml N_ECOML,
//             leng N_LENG,
//             _,
//         },
//     },
//     n_sect: u8,
//     n_desc: packed struct(u16) {
//         _pad0: u3 0,
//         arm_thumb_def: bool,
//         referenced_dynamically: bool,
//         /// The meaning of this bit is contextual.
//         /// See `N_DESC_DISCARDED` and `N_NO_DEAD_STRIP`.
//         discarded_or_no_dead_strip: bool,
//         weak_ref: bool,
//         /// The meaning of this bit is contextual.
//         /// See `N_WEAK_DEF` and `N_REF_TO_WEAK`.
//         weak_def_or_ref_to_weak: bool,
//         symbol_resolver: bool,
//         alt_entry: bool,
//         _pad2: u6 0,
//     },
//     n_value: u64,

//     pub fn tentative(sym: nlist_64) bool {
//         return sym.n_type.bits.type= .undf and sym.n_value != 0
//     }
// };

/// Format of a relocation entry of a Mach-O file.  Modified from the 4.3BSD
/// format.  The modifications from the original format were changing the value
/// of the r_symbolnum field for "local" (r_extern= 0) relocation entries.
/// This modification is required to support symbols in an arbitrary number of
/// sections not just the three sections (text, data and bss) in a 4.3BSD file.
/// Also the last 4 bits have had the r_type tag added to them.
// struct __attribute__((packed)) relocation_info_t {
//     /// offset in the section to what is being relocated
//     int32_t r_address;

//     /// symbol index if r_extern= 1 or section ordinal if r_extern= 0
//     r_symbolnum: u24,

//     /// was relocated pc relative already
//     r_pcrel: u1,

//     /// 0=byte, 1=word, 2=long, 3=quad
//     r_length: u2,

//     /// does not include value of sym referenced
//     r_extern: u1,

//     /// if not 0, machine specific relocation type
//     r_type: u4,
// };

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

/// CPU type targeting 64-bit Intel-based Macs
// #define CPU_TYPE_X86_64 0x01000007

/// CPU type targeting 64-bit ARM-based Macs
// #define CPU_TYPE_ARM64 0x0100000C

/// All Intel-based Macs
// #define CPU_SUBTYPE_X86_64_ALL 0x3

/// All ARM-based Macs
// #define CPU_SUBTYPE_ARM_ALL 0x0

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


// https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/osfmk/mach/machine.h
/*
 *	ARM subtypes
 */
/*
 * Capability bits used in the definition of cpu_type.
 */
#define CPU_ARCH_MASK           0xff000000      /* mask for architecture bits */
#define CPU_ARCH_ABI64          0x01000000      /* 64 bit ABI */
#define CPU_ARCH_ABI64_32       0x02000000      /* ABI for 64-bit hardware with 32-bit types; LP32 */

/*
 *	Machine types known by all.
 */

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
/* skip				((cpu_type_t) 19)	*/
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
#define CPU_SUBTYPE_X86_64_ALL          ((cpu_subtype_t)3)
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
