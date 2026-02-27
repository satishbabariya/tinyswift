"""
LLDB pretty-printers for TinySwift types.

Usage in LLDB:
  (lldb) command script import /path/to/lldb_formatters.py

This file provides synthetic children and summary providers for TinySwift's
runtime types so that values display readably in the debugger.

M122: LLDB Pretty-Printers for String, Array, Optional, Dictionary.
"""

import lldb


# =============================================================================
# String
# =============================================================================
# TinySwift strings are represented as i8* (pointer to null-terminated C string).
# In LLDB they show as raw pointers; this formatter dereferences to show text.

def string_summary(valobj, internal_dict):
    """Summary provider for TinySwift String (i8* / char*)."""
    try:
        # String is an opaque pointer — try to read as C string.
        addr = valobj.GetValueAsUnsigned(0)
        if addr == 0:
            return '""'
        process = valobj.GetProcess()
        error = lldb.SBError()
        # Read up to 256 bytes of the string.
        cstr = process.ReadCStringFromMemory(addr, 256, error)
        if error.Success() and cstr:
            return '"{}"'.format(cstr)
        return '<invalid string>'
    except Exception:
        return '<error reading string>'


# =============================================================================
# Optional
# =============================================================================
# TinySwift Optional<T> is lowered as { i1 has_value, T value }.

def optional_summary(valobj, internal_dict):
    """Summary provider for TinySwift Optional<T> ({i1, T} struct)."""
    try:
        has_value = valobj.GetChildMemberWithName('has_value')
        if has_value is None or not has_value.IsValid():
            # Try by index: field 0 = has_value, field 1 = value.
            has_value = valobj.GetChildAtIndex(0)
        if has_value is None or not has_value.IsValid():
            return '<invalid optional>'

        flag = has_value.GetValueAsUnsigned(0)
        if flag == 0:
            return '.none'

        value = valobj.GetChildMemberWithName('value')
        if value is None or not value.IsValid():
            value = valobj.GetChildAtIndex(1)
        if value is not None and value.IsValid():
            return '.some({})'.format(value.GetValue() or value.GetSummary() or '?')
        return '.some(?)'
    except Exception:
        return '<error reading optional>'


# =============================================================================
# Array
# =============================================================================
# TinySwift Array<T> is backed by a heap-allocated buffer with a count prefix.
# Layout: { i64 count, i64 capacity, T* buffer } or similar.
# This is a best-effort formatter that adapts to the actual struct layout.

class ArraySynthProvider:
    """Synthetic children provider for TinySwift Array<T>."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.count = 0

    def update(self):
        try:
            count_child = self.valobj.GetChildMemberWithName('count')
            if count_child is None or not count_child.IsValid():
                count_child = self.valobj.GetChildAtIndex(0)
            if count_child is not None and count_child.IsValid():
                self.count = count_child.GetValueAsUnsigned(0)
            else:
                self.count = 0
        except Exception:
            self.count = 0
        return True

    def num_children(self):
        return min(self.count, 100)  # Cap display at 100 elements.

    def get_child_index(self, name):
        try:
            if name.startswith('['):
                return int(name.strip('[]'))
        except Exception:
            pass
        return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= self.count:
            return None
        try:
            # Try to access buffer elements.
            buffer = self.valobj.GetChildMemberWithName('buffer')
            if buffer is None or not buffer.IsValid():
                buffer = self.valobj.GetChildAtIndex(2)
            if buffer is not None and buffer.IsValid():
                return buffer.GetChildAtIndex(index, lldb.eNoDynamicValues, True)
        except Exception:
            pass
        return None


def array_summary(valobj, internal_dict):
    """Summary provider for TinySwift Array<T>."""
    try:
        count_child = valobj.GetChildMemberWithName('count')
        if count_child is None or not count_child.IsValid():
            count_child = valobj.GetChildAtIndex(0)
        if count_child is not None and count_child.IsValid():
            count = count_child.GetValueAsUnsigned(0)
            return '{} element(s)'.format(count)
        return '<unknown array>'
    except Exception:
        return '<error reading array>'


# =============================================================================
# Dictionary
# =============================================================================

def dictionary_summary(valobj, internal_dict):
    """Summary provider for TinySwift Dictionary<K,V>."""
    try:
        count_child = valobj.GetChildMemberWithName('count')
        if count_child is None or not count_child.IsValid():
            count_child = valobj.GetChildAtIndex(0)
        if count_child is not None and count_child.IsValid():
            count = count_child.GetValueAsUnsigned(0)
            return '{} pair(s)'.format(count)
        return '<unknown dictionary>'
    except Exception:
        return '<error reading dictionary>'


# =============================================================================
# Registration
# =============================================================================

def __lldb_init_module(debugger, internal_dict):
    """Auto-registration hook called by LLDB on script import."""
    # String: match pointer types used for strings.
    debugger.HandleCommand(
        'type summary add -x "^String$" '
        '--python-function lldb_formatters.string_summary '
        '--category tinyswift')

    # Optional: match Optional struct types.
    debugger.HandleCommand(
        'type summary add -x "^Optional" '
        '--python-function lldb_formatters.optional_summary '
        '--category tinyswift')

    # Array summary.
    debugger.HandleCommand(
        'type summary add -x "^Array<" '
        '--python-function lldb_formatters.array_summary '
        '--category tinyswift')

    # Array synthetic children.
    debugger.HandleCommand(
        'type synthetic add -x "^Array<" '
        '--python-class lldb_formatters.ArraySynthProvider '
        '--category tinyswift')

    # Dictionary summary.
    debugger.HandleCommand(
        'type summary add -x "^Dictionary<" '
        '--python-function lldb_formatters.dictionary_summary '
        '--category tinyswift')

    # Enable the category.
    debugger.HandleCommand('type category enable tinyswift')

    print('[TinySwift] LLDB formatters loaded.')
