"""
LLDB initialization script for TinySwift debugging support.

This script auto-loads the TinySwift LLDB formatters when imported.
Add the following to your ~/.lldbinit file:

  command script import /path/to/lldb_init.py

Or import the formatters directly:

  command script import /path/to/lldb_formatters.py

M122: LLDB auto-load registration.
"""

import os
import lldb


def __lldb_init_module(debugger, internal_dict):
    """Auto-load TinySwift LLDB formatters from the same directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    formatters_path = os.path.join(script_dir, 'lldb_formatters.py')

    if os.path.exists(formatters_path):
        debugger.HandleCommand(
            'command script import "{}"'.format(formatters_path))
    else:
        print('[TinySwift] Warning: lldb_formatters.py not found at {}'.format(
            formatters_path))
