# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os.path
import re


ACRONYMS = [
    'CSSOM',
    'CSS',
    'DNS',
    'FE',
    'FTP',
    'HTML',
    'JS',
    'SVG',
    'URL',
    'WOFF',
    'XML',
    'XSLT',
    'XSS',
]


def lower_first(name):
    """Return name with first letter or initial acronym lowercased.

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            return name.replace(acronym, acronym.lower(), 1)
    return name[0].lower() + name[1:]


def upper_first(name):
    """Return name with first letter or initial acronym uppercased.
       The acronym must have a capital letter following it to be considered.
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym.lower()):
            if len(name) == len(acronym) or name[len(acronym)].isupper():
                return name.replace(acronym.lower(), acronym, 1)
    return upper_first_letter(name)


def upper_first_letter(name):
    """Return name with first letter uppercased."""
    if not name:
        return ''
    return name[0].upper() + name[1:]


def camel_case(css_name):
    """Convert hyphen-separated-name to UpperCamelCase.

    E.g., '-foo-bar' becomes 'FooBar'.
    """
    return ''.join(upper_first_letter(word) for word in css_name.split('-'))


def to_macro_style(name):
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()


def script_name(entry):
    return os.path.basename(entry['name'])


def cpp_name(entry):
    return entry['ImplementedAs'] or script_name(entry)


def enum_for_css_keyword(keyword):
    return 'CSSValue' + ''.join(camel_case(keyword))


def enum_for_css_property(property_name):
    return 'CSSProperty' + ''.join(camel_case(property_name))


def enum_for_css_property_alias(property_name):
    return 'CSSPropertyAlias' + camel_case(property_name)
