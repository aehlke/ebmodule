#!/usr/bin/env python
#
#  chujiten.py - an add-on program for xyaku
#                Tamito KAJIYAMA <12 September 2001>
#
#  $Id: chujiten.py,v 1.3 2001/09/22 23:46:51 kajiyama Exp $
#
#  NOTE: This file contains Japanese characters (EUC-JP).
#

from eb import *

import getopt
import string
import sys
import re

def get_content(book, appendix, hookset, folded, packed):
    buffer = []
    while 1:
        data = eb_read_text(book, appendix, hookset, None)
        if not data:
            break
        data = string.replace(data, "¢ª¡ø", "")
        data = string.replace(data, "¡á¢ª", "¡á")
        data = string.replace(data, "¢Í¢ª", "¢Í")
        data = string.replace(data, "¢Î¢ª", "¢Î")
        buffer.append(data)
    data = string.join(buffer, "")
    if packed:
        data = string.replace(data, "\n", "") + "\n"
    if folded < 0:
        return data
    i = end = 0
    j = len(data)
    buffer = []
    while i < j:
        if data[i] < "\200":
            n = 1
        else:
            n = 2
        if data[i] == "\n":
            buffer.append(data[end:i])
            end = i + 1
        elif i - end + n > folded:
            buffer.append(data[end:i])
            end = i
        i = i + n
    if end < j - 1:
        buffer.append(data[end:])
    return string.join(buffer, "\n") + "\n"

def hook_font(book, appendix, container, code, argv):
    gaiji = {
        (EB_HOOK_NARROW_FONT, 0xa121): "* ",
        (EB_HOOK_NARROW_FONT, 0xa122): "** ",
        (EB_HOOK_NARROW_FONT, 0xa123): "*** ",
        (EB_HOOK_NARROW_FONT, 0xa124): "o ",
        (EB_HOOK_NARROW_FONT, 0xa126): "¡Ô",
        (EB_HOOK_NARROW_FONT, 0xa127): "¡Õ",
        (EB_HOOK_NARROW_FONT, 0xa128): "¡Ì",
        (EB_HOOK_NARROW_FONT, 0xa129): "¡Í",
        (EB_HOOK_NARROW_FONT, 0xa12a): "¡Á",
        (EB_HOOK_NARROW_FONT, 0xa167): "a",
        (EB_HOOK_NARROW_FONT, 0xa168): "e",
        (EB_HOOK_NARROW_FONT, 0xa169): "i",
        (EB_HOOK_NARROW_FONT, 0xa16a): "o",
        (EB_HOOK_NARROW_FONT, 0xa16b): "u",
        (EB_HOOK_NARROW_FONT, 0xa16c): "y",
        (EB_HOOK_NARROW_FONT, 0xa16f): "I",
        (EB_HOOK_NARROW_FONT, 0xa17b): "a",
        (EB_HOOK_NARROW_FONT, 0xa17c): "e",
        (EB_HOOK_NARROW_FONT, 0xa17d): "i",
        (EB_HOOK_NARROW_FONT, 0xa17e): "o",
        (EB_HOOK_NARROW_FONT, 0xa221): "u",
        (EB_HOOK_NARROW_FONT, 0xa233): ":",
        (EB_HOOK_WIDE_FONT, 0xa34e): "¨¬",
        (EB_HOOK_WIDE_FONT, 0xa321): "[Ì¾]",
        (EB_HOOK_WIDE_FONT, 0xa322): "[Âå]",
        (EB_HOOK_WIDE_FONT, 0xa323): "[·Á]",
        (EB_HOOK_WIDE_FONT, 0xa324): "[Æ°]",
        (EB_HOOK_WIDE_FONT, 0xa325): "[Éû]",
        (EB_HOOK_WIDE_FONT, 0xa327): "[Á°]",
        (EB_HOOK_WIDE_FONT, 0xa32f): "[U]",
        (EB_HOOK_WIDE_FONT, 0xa330): "[C]",
        (EB_HOOK_WIDE_FONT, 0xa332): "(Ê£)",
        (EB_HOOK_WIDE_FONT, 0xa333): "[A]",
        (EB_HOOK_WIDE_FONT, 0xa334): "[P]",
        (EB_HOOK_WIDE_FONT, 0xa335): "(¼«)",
        (EB_HOOK_WIDE_FONT, 0xa336): "(Â¾)",
        (EB_HOOK_WIDE_FONT, 0xa337): "[À®",
        (EB_HOOK_WIDE_FONT, 0xa338): "¶ç]",
        (EB_HOOK_WIDE_FONT, 0xa32c): "[ÀÜ",
        (EB_HOOK_WIDE_FONT, 0xa32d): "Æ¬]",
        (EB_HOOK_WIDE_FONT, 0xa32e): "Èø]",
        (EB_HOOK_WIDE_FONT, 0xa339): "¡ø",
        (EB_HOOK_WIDE_FONT, 0xa33a): "¡ø",
        (EB_HOOK_WIDE_FONT, 0xa33c): "¡ø",
        (EB_HOOK_WIDE_FONT, 0xa34f): "¢Î",
        }
    eb_write_text_string(book, gaiji.get((code, argv[0]), "?"))
    return EB_SUCCESS

def make_candidates(word):
    match = re.search("[0-9A-Za-z.-]+", word)
    if match:
        word = match.group()
    buffer = [word]
    for suffix, replacement in [
        ("nning", "n"), ("rring", "r"), ("tting", "t"),
        ("ing", ""), ("ing", "e"),
        ("died", "dy"), ("fied", "fy"), ("ried", "ry"),
        ("nned", "n"), ("rred", "r"), ("tted", "t"),
        ("ed", "e"), ("ed", ""),
        ("fies", "fy"), ("ries", "ry"),
        ("es", "e"), ("es", ""),
        ("s", ""),
        ]:
        pos = -len(suffix)
        if word[pos:] == suffix:
            buffer.append(word[:pos] + replacement)
    return buffer

def main():
    # parse command-line arguments
    try:
        options, args = getopt.getopt(sys.argv[1:], "d:f:p")
    except getopt.error:
        sys.stderr.write("Usage: chujiten.py [options] [words]\n"
                         "Options:\n"
                         "-d DIR   dictionary directory\n"
                         "-f NUM   fold output at column NUM\n"
                         "-p       pack output\n")
        sys.exit(1)
    dictdir = "/usr/dict/chujiten"
    folded = -1
    packed = 0
    for opt, val in options:
        if opt == "-d":
            dictdir = val
        elif opt == "-f":
            folded = int(val)
        elif opt == "-p":
            packed = 1
    eb_initialize_library()
    book, appendix, hookset = EB_Book(), EB_Appendix(), EB_Hookset()
    eb_set_hooks(hookset, (
        (EB_HOOK_NARROW_FONT, hook_font),
        (EB_HOOK_WIDE_FONT,   hook_font)))
    try:
        eb_bind(book, dictdir)
    except EBError, (error, message):
        code = eb_error_string(error)
        sys.stderr.write("Error: %s: %s\n" % (code, message))
        sys.exit(1)
    eb_set_subbook(book, 0)
    if len(args) == 0:
        word = string.strip(sys.stdin.read())
    else:
        word = string.join(args)
    for word in make_candidates(word):
        found = 0
        eb_search_exactword(book, word)
        while 1:
            hitlist = eb_hit_list(book)
            if not hitlist:
                break
            found = 1
            for heading, text in hitlist:
                eb_seek_text(book, text)
                content = get_content(book, appendix, hookset, folded, packed)
                if string.strip(content):
                    sys.stdout.write(content)
        if found:
            break
    eb_finalize_library()

if __name__ == "__main__":
    main()
