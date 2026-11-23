#!/usr/bin/env python3
"""The RotWK functions the port never ports one by one: library, compiler and debug code (owner 2026-11-21).

The function map (tools/mod/rw_function_map.py, docs/ennorath/RW_FUNCTION_MAP.tsv) walks every module's call graph; a function
counts as ported only when engine/src cites its first instruction. The walk also reaches code the port replaces wholesale:
the C++ standard library the game linked (STLport containers, strings), the C runtime, compiler-generated helpers (deleting
destructors, SEH prologs), the game's debug hooks, and the engine infrastructure the port has its own of (the INI reader,
Xfer / CRC, the allocator: rw_function_map.HAND_STOPS). Such a function stays uncited forever; counting it as open work
overstates what is left. This tool writes their list, tools/mod/rw_library_functions.tsv (addresses and names only, no
game bytes); the map marks them `library` and the counts (progress_snapshot, MATRIX_GAPS, the task dossiers) leave them out.

    python tools/mod/rw_library_functions.py [--map docs/ennorath/RW_FUNCTION_MAP.tsv] [--out tools/mod/rw_library_functions.tsv]

The rule works on the map's `decomp` column (the BFME2 decomp's name of the matched function, Port 240) and is strict on
purpose: a container or string class of the standard library, a C runtime name (no class, leading underscore), a
compiler-generated destructor, the game's debug hooks, and the hand stop list. A game template (e.g.
`SparseMatchFinder<>`) is not library. A function with no decomp name is never listed here unless a task disassembles it
and records it in PROVEN_LIBRARY with its reason: otherwise it stays `none` until a task classifies it. A wrong entry
would hide a real gap: add by hand only with the reason, and review the diff.
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

STL = re.compile(r"(^|::|\b)(std::|_STL::|stlp_std::|_Tree|_Rb_tree|_Hashtable|_Ht_iterator<>|hashtable<>|basic_string|allocator|"
                 r"_Destroy<>|__destroy_aux<>|__uninitialized_copy<>|__uninitialized_fill_n<>|__copy<>|__copy_ptrs<>|_Construct<>|"
                 r"_M_create_node|_M_find<>|"
                 r"(map|multimap|vector|list|set|multiset|hash_map|hash_set|hash_multimap|deque|pair|less|equal_to)<>)|"
                 r"::(__copy|__copy_ptrs|__copy_backward|__copy_backward_ptrs|_Destroy|__destroy_aux|fill|uninitialized_fill_n|find|__find|_M_find)<>$|"
                 r"RefCountPtr<>::operator=$")
STRINGS = re.compile(r"(^|::)(AsciiString|UnicodeString|StringBase<>|EAStringC)(::|$)")
CRT = re.compile(r"^_{1,2}(SEH_prolog|SEH_epilog|memset|memcpy|memmove|memcmp|strcmp|strncmp|strcpy|strncpy|strcat|strncat|strlen|stricmp|"
                 r"strnicmp|atexit|atoi|atof|ftol2?|tolower|toupper|isprint|isspace|isalpha|isdigit|isalnum|upper_bound|lower_bound|"
                 r"security_check_cookie|CxxThrowException|purecall|allmul|alldiv|aulldiv|allrem|aullrem|chkstk|alloca_probe)$")
# the Lua 4 virtual machine the game links (its C API `_lua_*` and the internals around it): the port runs its own Lua
LUA_API = re.compile(r"^_lua[A-Za-z_]")
DEBUG = re.compile(r"^_bfme_debug|^DebugLog|^DEBUG_")
COMPILER = re.compile(r"`(scalar|vector) deleting destructor'")

# Functions proven by instruction-level disassembly in Local 135 (issue #852) to be library, allocator, string,
# container, or VM infrastructure that the port replaces wholesale.
PROVEN_LIBRARY: dict[int, tuple[str, str]] = {
    # String class (AsciiString / UnicodeString)
    0x0040511A: ("AsciiString::concat", "string class (AsciiString / UnicodeString)"),
    0x00405324: ("StringBase<char>::compareNoCase", "string class (AsciiString / UnicodeString)"),
    0x00435DE0: ("AsciiString::ensureUniqueBufferOfSize", "string class (AsciiString / UnicodeString)"),
    0x00455176: ("AsciiString::operator<", "string class (AsciiString / UnicodeString)"),
    0x008664B9: ("AsciiString::~AsciiString_memberThunk", "string class (AsciiString / UnicodeString)"),
    # Ref-counted smart pointers, memory pools, and spinlock / thread helpers
    0x00401DEB: ("MemoryPoolSingleBlock::allocateBlock", "port infrastructure or library (memory pool / allocator)"),
    0x0045121F: ("BfmePoolRef10::~BfmePoolRef10", "port infrastructure or library (ref-counted smart pointer)"),
    0x00464F76: ("SpinLock::lock", "port infrastructure or library (synchronization primitive)"),
    0x005D91D1: ("OpaqueRefElement4::operator=", "port infrastructure or library (ref-counted smart pointer)"),
    0x006633EE: ("LockedFreeList::push", "port infrastructure or library (memory pool / allocator)"),
    0x0069C0F1: ("MemoryPool::allocate", "port infrastructure or library (memory pool / allocator)"),
    0x0086393B: ("UpgradeModule::~UpgradeModule_baseThunk", "C runtime / compiler helper"),
    0x00A242B0: ("ThreadClass::Switch_Thread", "port infrastructure or library (synchronization primitive)"),
    # CRT / compiler / D3DX thunks
    0x00401399: ("__ehvec_ctor", "C runtime / compiler helper"),
    0x00A3CE22: ("__ArrayUnwind", "C runtime / compiler helper"),
    0x00A3CE80: ("__ehvec_dtor", "C runtime / compiler helper"),
    0x00A3CEC8: ("__ehvec_dtor_unwind", "C runtime / compiler helper"),
    0x00A3D282: ("_memmove", "C runtime / compiler helper"),
    0x00A3D79A: ("__strcmpi", "C runtime / compiler helper"),
    0x00A3ED38: ("D3DXVec3CatmullRom", "C runtime / compiler helper"),
    # STLport containers, iterators, and algorithms
    0x00423450: ("_STL::_Rb_global::_M_minimum", "C++ standard library container (STLport)"),
    0x00423470: ("_STL::_Rb_global::_M_maximum", "C++ standard library container (STLport)"),
    0x0042C320: ("_STL::make_pair<AsciiString,AsciiString>", "C++ standard library container (STLport)"),
    0x004BC9CF: ("_STL::_Destroy", "C++ standard library container (STLport)"),
    0x005DCEC8: ("_STL::_Rb_tree::_M_create_node", "C++ standard library container (STLport)"),
    0x005E0A39: ("_STL::hashtable::begin", "C++ standard library container (STLport)"),
    0x005EB0E5: ("_STL::list::_M_create_node", "C++ standard library container (STLport)"),
    0x005F19CD: ("_STL::vector::_M_fill_insert", "C++ standard library container (STLport)"),
    0x005FFE47: ("_STL::list::~list", "C++ standard library container (STLport)"),
    0x0060453C: ("_STL::pair::pair", "C++ standard library container (STLport)"),
    0x006051B0: ("_STL::make_pair", "C++ standard library container (STLport)"),
    0x006051CB: ("_STL::make_pair", "C++ standard library container (STLport)"),
    0x00606604: ("_STL::operator<<pair<AsciiString,AsciiString>>", "C++ standard library container (STLport)"),
    0x00606645: ("_STL::_Rb_tree::_M_create_node", "C++ standard library container (STLport)"),
    0x00606667: ("_STL::_Rb_tree::_M_create_node", "C++ standard library container (STLport)"),
    0x00610D25: ("_STL::find<const int*,int>", "C++ standard library container (STLport)"),
    0x006191BC: ("_STL::allocator::allocate", "C++ standard library container (STLport)"),
    0x0068CEBB: ("AttributeModifierPoolEntry::operator=", "C++ standard library container (STLport)"),
    0x0068FF46: ("_STL::__copy", "C++ standard library container (STLport)"),
    0x00691D82: ("_STL::__copy_ptrs", "C++ standard library container (STLport)"),
    0x00692F84: ("_STL::_Rb_tree::_M_insert", "C++ standard library container (STLport)"),
    0x00697BF0: ("_STL::_Destroy", "C++ standard library container (STLport)"),
    0x0069C0D4: ("_STL::__copy_ptrs", "C++ standard library container (STLport)"),
    0x006A88CF: ("_STL::pair<AsciiString&,AsciiString&>::operator=", "C++ standard library container (STLport)"),
    0x006ADB12: ("_STL::equal<AsciiString*>", "C++ standard library container (STLport)"),
    0x006AE86E: ("_STL::operator==<vector<AsciiString>>", "C++ standard library container (STLport)"),
    0x006D01C1: ("_STL::_Vector_base::_Vector_base", "C++ standard library container (STLport)"),
    0x006D04E6: ("_STL::_Rb_tree::_M_create_node", "C++ standard library container (STLport)"),
    0x006DA381: ("_STL::_Rb_tree::insert_unique", "C++ standard library container (STLport)"),
    0x0073B36C: ("_STL::vector<int>::_M_fill_insert", "C++ standard library container (STLport)"),
    0x0073C8B4: ("_STL::_Rb_tree::find", "C++ standard library container (STLport)"),
    0x0073DA77: ("_STL::_Rb_tree::_M_insert", "C++ standard library container (STLport)"),
    0x007628CF: ("_STL::allocator<ObjectFilter>::allocate", "C++ standard library container (STLport)"),
    0x007628FD: ("_STL::operator==<vector<int>>", "C++ standard library container (STLport)"),
    0x00762C65: ("ObjectFilter::~ObjectFilter", "C++ standard library container (STLport)"),
    0x00762CE5: ("ObjectFilter::ObjectFilter", "C++ standard library container (STLport)"),
    0x007630FA: ("_STL::_Construct<ObjectFilter>", "C++ standard library container (STLport)"),
    0x00763127: ("_STL::__uninitialized_copy<ObjectFilter*>", "C++ standard library container (STLport)"),
    0x00763156: ("_STL::__uninitialized_fill_n<ObjectFilter*>", "C++ standard library container (STLport)"),
    0x0076323B: ("_STL::__destroy_aux<ObjectFilter*>", "C++ standard library container (STLport)"),
    0x00763257: ("_STL::_Destroy<ObjectFilter*>", "C++ standard library container (STLport)"),
    0x007632E1: ("_STL::vector<ObjectFilter>::~vector", "C++ standard library container (STLport)"),
    0x007633E0: ("_STL::vector<ObjectFilter>::push_back", "C++ standard library container (STLport)"),
    0x00777E32: ("_STL::_List_node::allocate", "C++ standard library container (STLport)"),
    0x00794353: ("_STL::_List_node<AsciiString>::allocate", "C++ standard library container (STLport)"),
    0x0079FEAF: ("_STL::less<pair<NameKeyType,AsciiString>>", "C++ standard library container (STLport)"),
    0x007A14A0: ("_STL::_Rb_tree::find", "C++ standard library container (STLport)"),
    0x0080508C: ("AttributeModifierPoolEntry::AttributeModifierPoolEntry", "C++ standard library container (STLport)"),
    0x008051C5: ("AttributeModifierPoolEntry::AttributeModifierPoolEntry_copy", "C++ standard library container (STLport)"),
    0x008051F0: ("_STL::_Construct<AttributeModifierPoolEntry>", "C++ standard library container (STLport)"),
    0x0080521D: ("_STL::__uninitialized_copy<AttributeModifierPoolEntry*>", "C++ standard library container (STLport)"),
    0x00805243: ("_STL::__uninitialized_fill_n<AttributeModifierPoolEntry*>", "C++ standard library container (STLport)"),
    0x0081368A: ("_STL::hashtable::_M_bkt_num_key", "C++ standard library container (STLport)"),
    0x00828D5C: ("_STL::_STLP_alloc_proxy::_STLP_alloc_proxy", "C++ standard library container (STLport)"),
    0x0083692A: ("_STL::hashtable::find", "C++ standard library container (STLport)"),
    0x008E5A05: ("_STL::__copy", "C++ standard library container (STLport)"),
    0x008EA106: ("_STL::vector<void*>::_M_fill_insert", "C++ standard library container (STLport)"),
    0x008FAE55: ("_STL::__uninitialized_fill_n<int*>", "C++ standard library container (STLport)"),
    0x00945661: ("_STL::equal<const int*,const int*>", "C++ standard library container (STLport)"),
    0x00A3ABD0: ("_STL::__uninitialized_copy", "C++ standard library container (STLport)"),
    0x00A3AC20: ("_STL::__uninitialized_fill_n", "C++ standard library container (STLport)"),
    0x00A3AC70: ("_STL::__copy", "C++ standard library container (STLport)"),
    0x00A3B930: ("_STL::vector::_M_fill_insert", "C++ standard library container (STLport)"),
    # Lua 4 VM internals (within 0x00B5B180..0x00B64CC0)
    0x00B60E20: ("_luaD_openh", "Lua virtual machine (the port runs its own Lua)"),
    0x00B60F20: ("_callhook", "Lua virtual machine (the port runs its own Lua)"),
    0x00B60F80: ("_luaD_callC", "Lua virtual machine (the port runs its own Lua)"),
    0x00B60FF0: ("_luaD_call", "Lua virtual machine (the port runs its own Lua)"),
    0x00B616E0: ("_luaG_getline", "Lua virtual machine (the port runs its own Lua)"),
    0x00B61B40: ("_luaG_symbexec", "Lua virtual machine (the port runs its own Lua)"),
    0x00B61D00: ("_luaG_findobjname", "Lua virtual machine (the port runs its own Lua)"),
    0x00B62310: ("_luaO_equalObj", "Lua virtual machine (the port runs its own Lua)"),
    0x00B62420: ("_luaO_verror", "Lua virtual machine (the port runs its own Lua)"),
    0x00B62F20: ("_adjust_varargs", "Lua virtual machine (the port runs its own Lua)"),
    0x00B63030: ("_luaV_execute", "Lua virtual machine (the port runs its own Lua)"),
    0x00B64380: ("_newlstr", "Lua virtual machine (the port runs its own Lua)"),
    0x00B64650: ("_luaH_mainposition", "Lua virtual machine (the port runs its own Lua)"),
    0x00B64CC0: ("_luaM_realloc", "Lua virtual machine (the port runs its own Lua)"),
    # Apt EAStringC and pool allocator infrastructure
    0x00AE6A60: ("AptPool::getBlockFlag", "port infrastructure or library (Apt memory pool)"),
    0x00AE6B70: ("AptPool::allocBlock", "port infrastructure or library (Apt memory pool)"),
    0x00AE7040: ("EAStringC::FreeData", "string class (AsciiString / UnicodeString)"),
    0x00AE70C0: ("EAStringC::IsEmpty", "string class (AsciiString / UnicodeString)"),
    0x00AE70D0: ("EAStringC::GetLength", "string class (AsciiString / UnicodeString)"),
    0x00AE70E0: ("EAStringC::GetHash", "string class (AsciiString / UnicodeString)"),
    0x00AE7130: ("EAStringC::EAStringC", "string class (AsciiString / UnicodeString)"),
    0x00AE71B0: ("EAStringC::~EAStringC", "string class (AsciiString / UnicodeString)"),
    0x00AE71D0: ("EAStringC::operator=", "string class (AsciiString / UnicodeString)"),
    0x00AE7230: ("EAStringC::IsEqualTo", "string class (AsciiString / UnicodeString)"),
    0x00AE7610: ("EAStringC::Empty", "string class (AsciiString / UnicodeString)"),
    0x00AE7700: ("EAStringC::CompareNoCase", "string class (AsciiString / UnicodeString)"),
    0x00AE7890: ("EAStringC::IsEqualToNoCase", "string class (AsciiString / UnicodeString)"),
    0x00AE78F0: ("EAStringC::Reserve", "string class (AsciiString / UnicodeString)"),
    0x00AE7990: ("EAStringC::IsNotEmpty", "string class (AsciiString / UnicodeString)"),
    0x00AE79A0: ("EAStringC::ComputeHash", "string class (AsciiString / UnicodeString)"),
    0x00AE7D60: ("EAStringC::SetLength", "string class (AsciiString / UnicodeString)"),
    # Local 130 (issue #829) proven CRT, compiler, STLport, allocator, string, and smart-pointer helpers:
    0x0042C17A: ("STLport::_Construct<AsciiString>", "C++ standard library container (STLport)"),
    0x0042C1A7: ("STLport::uninitialized_fill_n<AsciiString*>", "C++ standard library container (STLport)"),
    0x0042C86C: ("STLport::_Destroy<AsciiString*>", "C++ standard library container (STLport)"),
    0x00430130: ("DynamicMemoryAllocator::rawAlloc(tag=3)", "port infrastructure or library (allocator wrapper [0xDC5E44])"),
    0x00430170: ("DynamicMemoryAllocator::rawFree(tag=3)", "port infrastructure or library (allocator wrapper [0xDC5E3C])"),
    0x004355D0: ("AsciiString::staticCriticalSection", "string class (AsciiString / UnicodeString)"),
    0x00435730: ("UnicodeString::staticCriticalSection", "string class (AsciiString / UnicodeString)"),
    0x004360C0: ("AsciiString::set(const char*, int)", "string class (AsciiString / UnicodeString)"),
    0x00436680: ("AsciiString::format_va", "string class (AsciiString / UnicodeString)"),
    0x004367B0: ("UnicodeString::releaseBuffer", "string class (AsciiString / UnicodeString)"),
    0x00440B21: ("SimpleCriticalSection::SimpleCriticalSection", "C runtime / compiler helper (InitializeCriticalSection)"),
    0x0044A2A5: ("BfmeWideResult::~BfmeWideResult", "port infrastructure or library (ref-counted query result handle)"),
    0x00450762: ("RefCounted::release", "port infrastructure or library (InterlockedDecrement smart-pointer release)"),
    0x0045486C: ("STLport::hash<AsciiString>::operator()", "C++ standard library container (STLport)"),
    0x004A86AC: ("Rva0036CA00Str::Rva0036CA00Str", "port infrastructure or library (InterlockedIncrement smart-pointer copy)"),
    0x004BB149: ("STLport::uninitialized_copy<AsciiString*>", "C++ standard library container (STLport)"),
    0x005FCBC4: ("AsciiString::releaseBuffer_thunk", "string class (AsciiString / UnicodeString)"),
    0x00645716: ("Rva002390CB::Rva002390CB", "port infrastructure or library (smart-pointer pair copy ctor)"),
    0x00671FB0: ("RefPtr<AudioEventInfo>::RefPtr", "port infrastructure or library (InterlockedIncrement smart-pointer ctor)"),
    0x00688E9D: ("STLport::allocator<void*>::allocate", "C++ standard library container (STLport)"),
    0x007AEAFD: ("SecondBase005D2111::~SecondBase005D2111", "compiler-generated empty base-interface destructor"),
    0x00917B0D: ("BfmeWideResultStorage::~BfmeWideResultStorage", "port infrastructure or library (ref-counted query result buffer free)"),
    0x009A2783: ("STLport::_M_create_node(0x18)", "C++ standard library container (STLport)"),
    0x00A340B0: ("STLport::__copy_trivial", "C++ standard library container (STLport)"),
    0x00A3A190: ("BfmeWideResult::BfmeWideResult", "port infrastructure or library (ref-counted query result ctor)"),
    0x00A3CD70: ("__chkstk", "C runtime / compiler helper"),
    0x00A3CF10: ("_strlen_thunk", "C runtime / compiler helper"),
    0x00A3CF22: ("_memcpy_thunk", "C runtime / compiler helper"),
    0x00A3CF4C: ("_onexit", "C runtime / compiler helper"),
    0x00A3D936: ("__dllonexit_thunk", "C runtime / compiler helper"),

    # Local 136 (fork issue 853): 127 shared helpers (56 EA Apt VM + 71 STLport/CRT/MemoryPool/AsciiString/smart pointers/debug).
    0x004050E6: ('AsciiString::set(const char*)', 'string class (AsciiString / UnicodeString)'),
    0x00424DA0: ('STLport::vector<void*>::vector(size_t)', 'C++ standard library container (STLport)'),
    0x0042C280: ('AsciiStringRef::write', 'string class (AsciiString / UnicodeString)'),
    0x00435F80: ('AsciiString::ensureUniqueBufferOfSize', 'string class (AsciiString / UnicodeString)'),
    0x00436370: ('AsciiString::concat', 'string class (AsciiString / UnicodeString)'),
    0x0044C43F: ('RvaSmartPtr12::attach', 'port infrastructure or library (intrusive smart-pointer list attach)'),
    0x0044C4BE: ('RvaSmartPtr12::RvaSmartPtr12', 'port infrastructure or library (intrusive smart-pointer copy ctor)'),
    0x00450315: ('STLport::_Destroy<GeometryShape*>', 'C++ standard library container (STLport)'),
    0x0045032E: ('STLport::vector<GeometryShape>::~vector', 'C++ standard library container (STLport)'),
    0x0045036D: ('STLport::vector<GeometryContactPoint>::~vector', 'C++ standard library container (STLport)'),
    0x00462DBA: ('GeometryShape::GeometryShape(copy)', 'compiler-generated trivial struct copy constructor'),
    0x00463394: ('MixFileCreator::FileInfoStruct::FileInfoStruct(copy)', 'compiler-generated trivial struct copy constructor'),
    0x004633E3: ('MixFileCreator::FileInfoStruct::operator=', 'compiler-generated trivial struct copy assignment'),
    0x00463468: ('STLport::_Construct<FileInfoStruct>', 'C++ standard library container (STLport)'),
    0x00463495: ('STLport::_Construct<BfmeStringRecord00063BE4>', 'C++ standard library container (STLport)'),
    0x00463A62: ('STLport::__uninitialized_copy<BfmeStringRecord00063BE4*>', 'C++ standard library container (STLport)'),
    0x00463A88: ('STLport::__uninitialized_copy<FileInfoStruct*>', 'C++ standard library container (STLport)'),
    0x0047D8B0: ('RefCounted::release', 'port infrastructure or library (ref-counted object release)'),
    0x004AD0EF: ('STLport::allocator<T(0x1C)>::allocate', 'C++ standard library container (STLport)'),
    0x004AD164: ('STLport::_Construct<BfmeStringRecord00111ACF>', 'C++ standard library container (STLport)'),
    0x004AD191: ('STLport::__uninitialized_copy<BfmeStringRecord00111ACF*>', 'C++ standard library container (STLport)'),
    0x004AF99C: ('STLport::__destroy_aux<Rva00111B25Record*>', 'C++ standard library container (STLport)'),
    0x004AFCE2: ('STLport::vector<Rva00111B25Record>::~vector', 'C++ standard library container (STLport)'),
    0x004B386C: ('AsciiStringRef::AsciiStringRef(const char*)', 'string class (AsciiString / UnicodeString)'),
    0x004B3D3B: ('AsciiStringRef::write(memcpy)', 'string class (AsciiString / UnicodeString)'),
    0x004B421B: ('AsciiStringPlusText::operator+', 'string class (AsciiString / UnicodeString)'),
    0x004BB514: ('AsciiStringPlusText::write', 'string class (AsciiString / UnicodeString)'),
    0x004BBCE9: ('AsciiStringPlusText::operator AsciiString', 'string class (AsciiString / UnicodeString)'),
    0x005017E1: ('BfmeStringRecord00111ACF::operator=', 'string class (AsciiString / UnicodeString)'),
    0x0050181D: ('STLport::__copy<BfmeStringRecord00111ACF*>', 'C++ standard library container (STLport)'),
    0x0050187A: ('STLport::vector::_M_allocate_and_copy<BfmeStringRecord00111ACF*>', 'C++ standard library container (STLport)'),
    0x005018A7: ('STLport::__copy_ptrs<BfmeStringRecord00111ACF*>', 'C++ standard library container (STLport)'),
    0x0051147C: ('BfmeStringRecord00111ACF::BfmeStringRecord00111ACF', 'string class (AsciiString / UnicodeString)'),
    0x005114D2: ('Rva00111B25Record::~Rva00111B25Record', 'string class (AsciiString / UnicodeString)'),
    0x005DCFE4: ('STLport::_Destroy_node<AsciiString>', 'C++ standard library container (STLport)'),
    0x005FFE1E: ('STLport::list::list()', 'C++ standard library container (STLport)'),
    0x006172F6: ('GeometryContactPoint::~GeometryContactPoint', 'string class (AsciiString / UnicodeString)'),
    0x00617839: ('STLport::_Destroy<GeometryContactPoint*>', 'C++ standard library container (STLport)'),
    0x0061924B: ('STLport::allocator<T(0x24)>::allocate', 'C++ standard library container (STLport)'),
    0x00655AA8: ('GeometryShape::~GeometryShape', 'string class (AsciiString / UnicodeString)'),
    0x006652A8: ('FreePooledNode', 'port infrastructure or library (MemoryPool::freeBlock)'),
    0x006652BE: ('STLport::__uninitialized_copy<BfmeStringRecord0022074B*>', 'C++ standard library container (STLport)'),
    0x00686160: ('MemoryPool::allocateBlock(0x10)', 'port infrastructure or library (MemoryPool allocator)'),
    0x0068683B: ('Rva00286136Free', 'port infrastructure or library (MemoryPool::freeBlock)'),
    0x006868A8: ('STLport::_Rb_tree::_M_erase', 'C++ standard library container (STLport)'),
    0x00686F85: ('STLport::_Rb_tree::~_Rb_tree', 'C++ standard library container (STLport)'),
    0x00691435: ('empty_ret4_stub', 'compiler-generated empty virtual/callback stub (ret 4)'),
    0x006BF945: ('STLport::allocator<T(0x0C)>::allocate', 'C++ standard library container (STLport)'),
    0x006CF86E: ('_fprintf_appendLine', 'debug hook of the game'),
    0x006DD0AB: ('AsciiStringPlusText::length', 'string class (AsciiString / UnicodeString)'),
    0x007194D6: ('STLport::__copy_ptrs_dword_loop', 'C++ standard library container (STLport)'),
    0x00734B9A: ('STLport::__copy_ptrs<Rva00335C62Element*>', 'C++ standard library container (STLport)'),
    0x0075A646: ('Rva00358B65::~Rva00358B65', 'port infrastructure or library (ref-counted string-pair dtor)'),
    0x0075B554: ('STLport::iterator_copy_ctor_4B', 'C++ standard library container (STLport)'),
    0x0076293A: ('ObjectFilter::poolRelease', 'port infrastructure or library (ObjectFilter intern-pool handle release)'),
    0x007629B0: ('ObjectFilter::~ObjectFilter', 'port infrastructure or library (ObjectFilter intern-pool handle dtor)'),
    0x0076EC18: ('STLport::find<list<UnicodeString>::iterator>', 'C++ standard library container (STLport)'),
    0x007EE233: ('STLport::_List_base::_M_create_node(0x0C)', 'C++ standard library container (STLport)'),
    0x00831762: ('STLport::_Construct<pair>', 'C++ standard library container (STLport)'),
    0x00865C2C: ('STLport::_Rb_tree::_M_erase', 'C++ standard library container (STLport)'),
    0x00865C59: ('STLport::_Rb_tree::_M_erase', 'C++ standard library container (STLport)'),
    0x0086677B: ('STLport::_Rb_tree::~_Rb_tree', 'C++ standard library container (STLport)'),
    0x008667B3: ('STLport::_Rb_tree::~_Rb_tree', 'C++ standard library container (STLport)'),
    0x00866E73: ('STLport::_Rb_tree::_M_erase', 'C++ standard library container (STLport)'),
    0x00867305: ('STLport::_Rb_tree::clear', 'C++ standard library container (STLport)'),
    0x008675B0: ('STLport::_Rb_tree::~_Rb_tree', 'C++ standard library container (STLport)'),
    0x008C18AC: ('InterfaceBase::~InterfaceBase', 'compiler-generated empty base-interface destructor'),
    0x008E3EE9: ('STLport::vector::deallocate', 'C++ standard library container (STLport)'),
    0x00A110BB: ('RefPtr::reset', 'port infrastructure or library (ref-counted smart-pointer reset)'),
    0x00A3D28E: ('__ehvec_ctor', 'C runtime / compiler helper'),
    0x00A3D2D8: ('__ehvec_dtor', 'C runtime / compiler helper'),
    0x00AE7EB0: ('AptVM::sub_00AE7EB0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AE8C40: ('EAStringC::ChangeBuffer', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AE8D90: ('AptVM::sub_00AE8D90', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AE8E20: ('EAStringC::EAStringC', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEA3D0: ('BfmeStrVKK::bfmeTruncVKK', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEAE60: ('Rva006D6D20::Rva006D6D20', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEB3C0: ('AptString::Create', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEF190: ('Rva006DB160::rva006DAFE0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEF240: ('Rva006DB090::rva006DB090', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEF310: ('AptVM::sub_00AEF310', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEF420: ('Rva006DB270::freeBlock', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEF5C0: ('bfmeSkipVF', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFBD0: ('Rva00897FD0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFCF0: ('AptValue::getRefCount', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFD00: ('Rva006DBB30SarDwordField::get', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFD90: ('BfmeAptValue006DCD20::setTypeAt006DBBC0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFE40: ('BfmeAptValue006DCD20::setGCRootCount', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFF80: ('Rva006DBDB0DwordOrSetter::apply', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFF90: ('Rva006DBDC0DwordMask::clear', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AEFFA0: ('Rva006DBDD0ShrNAndField::get', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF01E0: ('AptVM::sub_00AF01E0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0250: ('AptVM::sub_00AF0250', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0390: ('AptVM::sub_00AF0390', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0430: ('AptVM::sub_00AF0430', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0480: ('AptVM::sub_00AF0480', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0750: ('AptVM::sub_00AF0750', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0E90: ('BfmeAptValue006DCD20::BfmeAptValue006DCD20', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF0EF0: ('BfmeAptValue006DCD20::BfmeAptValue006DCD20', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF10B0: ('BfmeAptValue006DCD20::rva006DCEE0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF10F0: ('BfmeAptValue006DCD20::rva006DCF20', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF1130: ('BfmeAptValue006DCD20::rva006DCF60', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF1890: ('AptVM::sub_00AF1890', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF2A40: ('AptValue::rva006DE870', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF3B70: ('AptVM::sub_00AF3B70', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF44A0: ('AptVM::sub_00AF44A0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF4590: ('AptVM::sub_00AF4590', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF7DA0: ('AptVM::sub_00AF7DA0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AF7DE0: ('AptVM::sub_00AF7DE0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00AFAF10: ('AptValueVector::rva006E6C00', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B0ADB0: ('AptVM::sub_00B0ADB0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B0AF60: ('AptVM::sub_00B0AF60', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B0B1B0: ('BfmeQuery1279::rva006F6FB0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B0FDD0: ('Rva006FBBE0::find', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B10070: ('Rva006FBE80::find2', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B112F0: ('AptVM::sub_00B112F0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B12270: ('AptVM::sub_00B12270', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B128A0: ('AptVM::sub_00B128A0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B12DB0: ('rva006FEC00', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B13F30: ('AptVM::sub_00B13F30', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1A810: ('AptVM::sub_00B1A810', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1D8E0: ('AptVM::sub_00B1D8E0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1D960: ('AptVM::sub_00B1D960', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1F140: ('AptNativeHash::HashFindKey', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1F530: ('Rva0070B380::lookup', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B1F6A0: ('Rva0070B4F0GetString', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
    0x00B6CEE0: ('AptVM::sub_00B6CEE0', 'EA Apt Flash/ActionScript VM (the port uses native Godot UI)'),
}


def classify(name: str, address: int = 0, lua_block: tuple[int, int] = (0, 0)) -> str | None:
    if address in PROVEN_LIBRARY:
        return PROVEN_LIBRARY[address][1]
    n = re.sub(r"\s*\(tier [A-Z]\)\s*$", "", name or "").strip()
    if not n:
        return None
    if DEBUG.search(n):
        return "debug hook of the game"
    if COMPILER.search(n):
        return "compiler-generated deleting destructor"
    if STL.search(n):
        return "C++ standard library container (STLport)"
    if STRINGS.search(n):
        return "string class (AsciiString / UnicodeString)"
    if "::" not in n and (CRT.match(n) or n.startswith("operator new") or n.startswith("operator delete")):
        return "C runtime / compiler helper"
    if "::" not in n and (LUA_API.match(n) or (0 < lua_block[0] <= address <= lua_block[1] and n.startswith("_") and not n.startswith("_bfme"))):
        return "Lua virtual machine (the port runs its own Lua)"
    return None



# Proven by RotWK 2.01 disassembly (Local 133, client / draw classes): STLport container/string internals,
# WW3D DynamicVectorClass/RefCountPtr/HashTableClass, allocator free, and compiler-generated record helpers
# whose decomp names are unnamed or generic Rva*/Bfme* names.
PROVEN_CLIENT_DRAW_LIBRARY: dict[int, tuple[str, str]] = {
    # W3DHordeModelDraw callees (STLport basic_string, _Rb_tree, list, AsciiString, RefCountClass, record copy/dtor)
    0x00407410: ("std::basic_string<char>::basic_string", "C++ standard library container (STLport)"),
    0x004088C0: ("std::basic_string<char>::_M_range_initialize", "C++ standard library container (STLport)"),
    0x00408D30: ("std::basic_string<char>::basic_string(copy)", "C++ standard library container (STLport)"),
    0x00411D30: ("std::basic_string<char>::erase", "C++ standard library container (STLport)"),
    0x0041B010: ("std::basic_string<char>::append", "C++ standard library container (STLport)"),
    0x004363B0: ("AsciiString::toLower", "string class (AsciiString / UnicodeString)"),
    0x00454778: ("_Rb_tree::_M_create_node", "C++ standard library container (STLport)"),
    0x00478553: ("Rva00752630::Rva00752630(copy)", "compiler-generated struct copy constructor"),
    0x00478A7C: ("Rva000791EDCopy", "C++ standard library container (STLport)"),
    0x00478E00: ("BfmeNarrowRecord00079C23::~BfmeNarrowRecord00079C23", "compiler-generated struct destructor"),
    0x0047900B: ("BfmeNarrowRecord00079C23::BfmeNarrowRecord00079C23", "compiler-generated struct constructor"),
    0x0047946B: ("BfmeNarrowRecord00079C23::BfmeNarrowRecord00079C23(copy)", "compiler-generated struct copy constructor"),
    0x00479A90: ("BfmeNarrowRecord00079C23::operator=", "compiler-generated struct assignment"),
    0x00479CFE: ("BfmeHelperYS::BfmeHelperYS", "compiler-generated struct constructor"),
    0x004B5C5C: ("_Rb_tree::_M_erase", "C++ standard library container (STLport)"),
    0x004B8A90: ("_Rb_tree::clear", "C++ standard library container (STLport)"),
    0x004BB948: ("std::list::erase", "C++ standard library container (STLport)"),
    0x00778D70: ("std::__find(AsciiString)", "C++ standard library container (STLport)"),
    0x00778E1C: ("std::find(AsciiString)", "C++ standard library container (STLport)"),
    0x007985FC: ("_Rb_tree::_M_get_node(header)", "C++ standard library container (STLport)"),
    0x00806CFF: ("std::_Destroy(0x18)", "C++ standard library container (STLport)"),
    0x0082147E: ("std::list::_M_create_node(sentinel)", "C++ standard library container (STLport)"),
    0x00A329C0: ("RefCountClass::Release_Ref", "WW3D ref-counted smart pointer"),
    # W3DSupplyDraw callees (STLport vector/_Rb_tree destructors and compiler-generated record assign/dtor)
    0x004B3C30: ("BfmeRecord000B43E5::operator=", "compiler-generated struct assignment"),
    0x004B604C: ("Rva000B978ARecord::operator=", "compiler-generated struct assignment"),
    0x004B927E: ("Rva000B9AAA::~Rva000B9AAA", "compiler-generated struct destructor"),
    0x004B9296: ("Rva00B9AC2::~Rva00B9AC2", "compiler-generated struct destructor"),
    0x004BAE30: ("_Rb_tree::~_Rb_tree", "C++ standard library container (STLport)"),
    0x004BCA33: ("std::_Destroy(Rva00B9AC2)", "C++ standard library container (STLport)"),
    0x004BD963: ("BfmeVectorRecord000BDF17::~BfmeVectorRecord000BDF17", "compiler-generated struct destructor"),
    0x004BE552: ("Rva000BEDF0Record::~Rva000BEDF0Record", "compiler-generated struct destructor"),
    0x004BFB46: ("std::vector::~vector(AnimSet)", "C++ standard library container (STLport)"),
    0x004BFB85: ("std::vector::~vector(StringTail)", "C++ standard library container (STLport)"),
    0x004BFBC4: ("std::vector::~vector(AsciiString)", "C++ standard library container (STLport)"),
    0x004C046E: ("BfmeStringTailRecord180::~BfmeStringTailRecord180", "compiler-generated struct destructor"),
    0x004C12AC: ("std::vector::~vector(Rva00B9AC2)", "C++ standard library container (STLport)"),
    0x004C12EB: ("std::vector::~vector(Rva000BDD08)", "C++ standard library container (STLport)"),
    0x004C1335: ("std::vector::~vector(Rva00079554Record)", "C++ standard library container (STLport)"),
    0x004C204B: ("BfmePod252::~BfmePod252", "compiler-generated struct destructor"),
    0x004C643E: ("BfmePod248::~BfmePod248", "compiler-generated struct destructor"),
    0x004C827D: ("std::vector::~vector(BfmePod248)", "C++ standard library container (STLport)"),
    0x006CFD22: ("Rva000B435F::operator=", "compiler-generated struct assignment"),
    0x006D049C: ("std::__copy_ptrs(Rva000B435F)", "C++ standard library container (STLport)"),
    # W3DDebrisDraw callees (AsciiString/SplitString/HashTableClass/CRC_Stringi/RefCountPtr)
    0x004356B0: ("skipSeps", "string class (AsciiString / UnicodeString)"),
    0x004356F0: ("skipNonSeps", "string class (AsciiString / UnicodeString)"),
    0x00507E74: ("Ref_Ptr_Set", "WW3D ref-counted smart pointer"),
    0x00508511: ("AsciiStringCharPlusText::make", "string class (AsciiString / UnicodeString)"),
    0x00509687: ("AsciiStringCharPlusText::write", "string class (AsciiString / UnicodeString)"),
    0x0050B479: ("AsciiStringCharPlusText::operator AsciiString", "string class (AsciiString / UnicodeString)"),
    0x0053189B: ("RefCountPtr<TextureClass>::RefCountPtr", "WW3D ref-counted smart pointer"),
    0x00536B39: ("SplitString", "string class (AsciiString / UnicodeString)"),
    0x0091507F: ("AsciiStringCharPlusText::operator+", "string class (AsciiString / UnicodeString)"),
    0x00A279E0: ("HashTableClass::Find", "WW3D container (HashTableClass)"),
    0x00A29500: ("CRC_Stringi", "WW3D hash helper (CRC_Stringi)"),
    # W3DLaserDraw callees (allocator free, DynamicVectorClass, RefCountPtr<TextureClass> vector dtor)
    0x0042F6C0: ("operator delete (pool)", "C runtime / compiler helper"),
    0x0048FA88: ("DynamicVectorClass::Resize", "WW3D container (DynamicVectorClass)"),
    0x004C9105: ("std::_Destroy(RefCountPtr<TextureClass>)", "C++ standard library container (STLport)"),
    0x004C911E: ("std::vector<RefCountPtr<TextureClass>>::~vector", "C++ standard library container (STLport)"),
    0x004FFBF2: ("DynamicVectorClass<Vector3>::Add", "WW3D container (DynamicVectorClass)"),
    0x00525D79: ("RefCountPtr<TextureClass>::~RefCountPtr", "WW3D ref-counted smart pointer"),
    # W3DStreakDraw callees (vector dtor, DynamicVectorClass)
    0x004CFCD4: ("std::vector<AsciiString>::~vector", "C++ standard library container (STLport)"),
    0x004FFB3D: ("DynamicVectorClass::Shrink", "WW3D container (DynamicVectorClass)"),
    0x004FFC33: ("DynamicVectorClass<Vector3>::Delete", "WW3D container (DynamicVectorClass)"),
    0x00567C1D: ("DynamicVectorClass<float>::Add", "WW3D container (DynamicVectorClass)"),
    0x00567C50: ("DynamicVectorClass<float>::Delete", "WW3D container (DynamicVectorClass)"),
    # TerrainResourceClientBehavior callees (_Rb_tree / std::list)
    0x008EC278: ("_Rb_tree::_M_erase", "C++ standard library container (STLport)"),
    0x008EC2EF: ("_Rb_tree::clear", "C++ standard library container (STLport)"),
    0x008EC35C: ("std::list::clear", "C++ standard library container (STLport)"),
    # TransitionDamageFX callees (vector<RubbleNeighbor> dtor)
    0x008BE311: ("RubbleNeighbor::~RubbleNeighbor", "compiler-generated struct destructor"),
    0x008BE48A: ("std::_Destroy(RubbleNeighbor)", "C++ standard library container (STLport)"),
    0x008BE4A3: ("std::vector<RubbleNeighbor>::~vector", "C++ standard library container (STLport)"),
    # W3DQuadrupedDraw callee (std::vector::_M_fill_insert)
    0x004C98D5: ("std::vector<Coord3D>::_M_fill_insert", "C++ standard library container (STLport)"),
    # BeaconClientUpdate callee (RvaSmartPtr12::operator=)
    0x0044C4E2: ("RvaSmartPtr12::operator=", "WW3D ref-counted smart pointer"),
}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--map", type=Path, default=Path("docs/ennorath/RW_FUNCTION_MAP.tsv"))
    ap.add_argument("--out", type=Path, default=Path("tools/mod/rw_library_functions.tsv"))
    a = ap.parse_args(argv)
    from rw_function_map import HAND_STOPS  # here, not at import: load() is used without capstone
    rows = {}
    for address, name in HAND_STOPS.items():
        rows[address] = (name, "port infrastructure or library (rw_function_map.HAND_STOPS)")
    for address, (name, why) in PROVEN_LIBRARY.items():
        if address not in rows:
            rows[address] = (name, why)
    for address, (name, why) in PROVEN_CLIENT_DRAW_LIBRARY.items():
        rows.setdefault(address, (name, why))
    with a.map.open(encoding="utf-8") as f:
        table = list(csv.DictReader(f, delimiter="\t"))
    bare = lambda r: re.sub(r"\s*\(tier [A-Z]\)\s*$", "", r.get("decomp") or "").strip()
    lua = [int(r["function"], 16) for r in table if LUA_API.match(bare(r))]
    lua_block = (min(lua), max(lua)) if lua else (0, 0)  # the span of the `_lua*` functions: the VM's own code
    for r in table:
        address = int(r["function"], 16)
        why = classify(r.get("decomp", ""), address, lua_block)
        if why and address not in rows:
            label = bare(r) or (PROVEN_LIBRARY[address][0] if address in PROVEN_LIBRARY else f"sub_{address:08X}")
            rows[address] = (label, why)
    for address, (label, why) in PROVEN_LIBRARY.items():
        if address not in rows:
            rows[address] = (label, why)
    with a.out.open("w", encoding="utf-8", newline="") as f:
        f.write("# RotWK functions the port replaces wholesale (tools/mod/rw_library_functions.py): the function map marks them `library`.\n")
        f.write("address\tname\treason\n")
        for address in sorted(rows):
            name, why = rows[address]
            f.write(f"0x{address:08X}\t{name}\t{why}\n")
    print(f"{a.out}: {len(rows)} functions")
    return 0


def load(path: Path = Path(__file__).resolve().parent / "rw_library_functions.tsv") -> dict[int, str]:
    """address -> reason, for the map and the counts; empty when the list is missing."""
    if not path.exists():
        return {}
    out = {}
    with path.open(encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or line.startswith("address\t") or not line.strip():
                continue
            address, _name, why = line.rstrip("\n").split("\t")
            out[int(address, 16)] = why
    return out


if __name__ == "__main__":
    sys.exit(main())
