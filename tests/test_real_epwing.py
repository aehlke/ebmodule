"""Real EB4j EPWING regressions; set EB_TEST_FIXTURE_ROOT to the retained fixtures directory.

Upstream: eb4j/eb4j bccd2f8b852928a42076e7af54675c7240544364.
Keep its data License.txt and repository LGPL notice with the external packet.
The dictionaries contain word indexes, not endword/keyword indexes or authored media.
"""
import hashlib
import os
from pathlib import Path
import unittest

import eb
import eblib


class RealEPWINGTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        location = os.environ.get("EB_TEST_FIXTURE_ROOT")
        if not location:
            raise unittest.SkipTest("real EPWING gate requires EB_TEST_FIXTURE_ROOT")
        cls.root = Path(location).resolve(strict=True)
        expected = {
            "License.txt": "e824adfb0d2858158943a857e41bc0f9725ddf7b7070739063f295063bea1f58",
            "epwing/chimei/DATA/HONMON": "435fcc720554f9463360ea2e63cce7a342b9b513f0c0f2219cba295b53ca5e00",
            "epwing-zipped/chimei/DATA/HONMON.ebz": "eb2f0b9c7272a65f8ec72cc7e7b32bcb568034d34041a290cec3f12d64380c1b",
            "epwing-gaiji/JISYO/DATA/HONMON": "567767ec81eac40a2c6ee5f17b904f4fa84502c987a12690ec5b73f98b1a55d9",
            "epwing-gaiji/JISYO/GAIJI/GAI16F00": "6d00638fc813c9db2d5478d6f5e559ad00f1a823d6f152cfa842c6b7a15940a0",
            "epwing-gaiji/JISYO/GAIJI/GAI16H00": "1c118d507e89786506f5a363fedc71e0cdd5f677f2aa33e17cd041f62c78e637",
        }
        for name, digest in expected.items():
            actual = hashlib.sha256((cls.root / name).read_bytes()).hexdigest()
            if actual != digest:
                raise AssertionError("fixture hash differs: " + name)
        eb.eb_initialize_library()

    @classmethod
    def tearDownClass(cls):
        eb.eb_finalize_library()

    def open_book(self, name="epwing"):
        book = eb.EB_Book()
        eb.eb_bind(book, str(self.root / name))
        subbooks = eb.eb_subbook_list(book)
        self.assertTrue(subbooks)
        selected = next(code for code in subbooks
                        if eb.eb_subbook_directory2(book, code).lower() == b"chimei")
        eb.eb_set_subbook(book, selected)
        return book

    def find_tokyo(self, book, search=eb.eb_search_exactword, query=b"Tokyo"):
        search(book, query)
        hits = eb.eb_hit_list(book)
        self.assertEqual(len(hits), 1)
        self.assertEqual(eb.eb_hit_list(book), ())
        return hits[0]

    def chunks(self, book, position, reader, hooks=None, container=None):
        appendix = eb.EB_Appendix()
        if hooks is None:
            hooks = eb.EB_Hookset()
        eb.eb_seek_text(book, position)
        chunks = []
        for _ in range(100):
            chunk = reader(book, appendix, hooks, container)
            self.assertIsInstance(chunk, bytes)
            if not chunk:
                return chunks
            chunks.append(chunk)
        self.fail("text read did not reach the end")

    def test_exactword_prefix_and_text_on_plain_and_compressed_books(self):
        for name in ("epwing", "epwing-zipped"):
            with self.subTest(book=name):
                book = self.open_book(name)
                self.assertTrue(eb.eb_have_exactword_search(book))
                self.assertTrue(eb.eb_have_word_search(book))
                exact = self.find_tokyo(book)
                prefix = self.find_tokyo(book, eb.eb_search_word, b"Tok")
                self.assertEqual(exact, prefix)
                heading = b"".join(self.chunks(book, exact[0], eb.eb_read_heading))
                text = b"".join(self.chunks(book, exact[1], eb.eb_read_text))
                self.assertEqual(heading.decode("euc_jp"), "Tokyo")
                self.assertIn("東京", text.decode("euc_jp"))

    def test_absent_endword_and_keyword_indexes_report_their_real_limit(self):
        book = self.open_book()
        self.assertFalse(eb.eb_have_endword_search(book))
        self.assertFalse(eb.eb_have_keyword_search(book))
        for search, query in ((eb.eb_search_endword, b"Tokyo"),
                              (eb.eb_search_keyword, (b"Tokyo",))):
            with self.subTest(search=search.__name__):
                with self.assertRaises(eb.EBError) as raised:
                    search(book, query)
                self.assertEqual(raised.exception.args[0], eb.EB_ERR_NO_SUCH_SEARCH)

    def test_high_level_search_and_stop_code_hook(self):
        dictionary = eblib.EB(str(self.root / "epwing"))
        dictionary.set_subbook(next(code for code in dictionary.subbook_list()
            if dictionary.subbook_directory(code).lower() == b"chimei"))
        hit, = dictionary.search_exactword("Tokyo")
        self.assertEqual(hit.heading(), b"Tokyo")
        self.assertIn("東京", hit.text().decode("euc_jp"))

    def test_hooks_keep_container_and_preserve_exceptions_and_recover(self):
        class HookFailure(Exception):
            pass

        book = self.open_book()
        heading, _ = self.find_tokyo(book)
        hooks = eb.EB_Hookset()
        container = object()
        failure = HookFailure("authored failure")
        seen = []

        def broken(c_book, appendix, received, code, argv):
            seen.append((c_book, received, code, argv))
            raise failure

        eb.eb_set_hook(hooks, (eb.EB_HOOK_NARROW_JISX0208, broken))
        with self.assertRaises(HookFailure) as raised:
            self.chunks(book, heading, eb.eb_read_heading, hooks, container)
        self.assertIs(raised.exception, failure)
        self.assertIs(seen[0][0], book)
        self.assertIs(seen[0][1], container)
        self.assertEqual(seen[0][2], eb.EB_HOOK_NARROW_JISX0208)
        self.assertTrue(all(isinstance(value, int) for value in seen[0][3]))
        eb.eb_set_hook(hooks, (eb.EB_HOOK_NARROW_JISX0208, eb.eb_hook_euc_to_ascii))
        self.assertEqual(b"".join(self.chunks(book, heading, eb.eb_read_heading, hooks)), b"Tokyo")
        eb.eb_set_hook(hooks, (eb.EB_HOOK_NARROW_JISX0208, lambda *args: "bad status"))
        with self.assertRaises(TypeError):
            self.chunks(book, heading, eb.eb_read_heading, hooks)
        eb.eb_set_hook(hooks, (eb.EB_HOOK_NARROW_JISX0208, lambda *args: 2 ** 100))
        with self.assertRaises(OverflowError):
            self.chunks(book, heading, eb.eb_read_heading, hooks)

    def test_failed_hook_batch_does_not_install_a_partial_callback(self):
        book = self.open_book()
        heading, _ = self.find_tokyo(book)
        hooks = eb.EB_Hookset()
        with self.assertRaises(eb.EBError):
            eb.eb_set_hooks(hooks, ((eb.EB_HOOK_NARROW_JISX0208, lambda *args: eb.EB_SUCCESS),
                                    (100000, lambda *args: eb.EB_SUCCESS)))
        self.assertEqual(b"".join(self.chunks(book, heading, eb.eb_read_heading, hooks)), b"Tokyo")

    def test_nested_read_is_rejected_before_entering_the_native_cache(self):
        book, other = self.open_book(), self.open_book()
        heading, _ = self.find_tokyo(book)
        other_heading, _ = self.find_tokyo(other)
        eb.eb_seek_text(other, other_heading)
        appendix, other_hooks, hooks = eb.EB_Appendix(), eb.EB_Hookset(), eb.EB_Hookset()
        calls = []

        def nested(*args):
            with self.assertRaises(RuntimeError):
                eb.eb_read_heading(other, appendix, other_hooks, None)
            for target in (book, other):
                with self.assertRaises(RuntimeError):
                    eb.eb_forward_text(target, appendix)
                with self.assertRaises(RuntimeError):
                    eb.eb_read_rawtext(target, 32)
            calls.append(True)
            return eb.eb_hook_euc_to_ascii(*args)

        eb.eb_set_hook(hooks, (eb.EB_HOOK_NARROW_JISX0208, nested))
        self.assertEqual(b"".join(self.chunks(book, heading, eb.eb_read_heading, hooks)), b"Tokyo")
        self.assertTrue(calls)

    def test_multibyte_hook_output_crosses_native_chunk_boundary(self):
        book = self.open_book()
        heading, _ = self.find_tokyo(book)
        hooks = eb.EB_Hookset()
        first = [True]

        def begin(c_book, *args):
            eb.eb_write_text(c_book, b"x" * 2046 + b"\xc6")
            return eb.EB_SUCCESS

        def character(c_book, *args):
            if first[0]:
                first[0] = False
                eb.eb_write_text(c_book, b"\xfc")
                return eb.EB_SUCCESS
            return eb.eb_hook_euc_to_ascii(c_book, *args)

        eb.eb_set_hooks(hooks, ((eb.EB_HOOK_BEGIN_NARROW, begin),
                               (eb.EB_HOOK_NARROW_JISX0208, character)))
        chunks = self.chunks(book, heading, eb.eb_read_heading, hooks)
        self.assertGreaterEqual(len(chunks), 2)
        self.assertTrue(chunks[0].endswith(b"\xc6"))
        self.assertTrue(chunks[1].startswith(b"\xfc"))
        self.assertEqual(b"".join(chunks).decode("euc_jp"), "x" * 2046 + "日okyo")

    def test_deferred_two_byte_write_preserves_both_bytes(self):
        book = self.open_book()
        heading, _ = self.find_tokyo(book)
        hooks = eb.EB_Hookset()
        first = [True]

        def begin(c_book, *args):
            eb.eb_write_text(c_book, b"x" * 2046)
            return eb.EB_SUCCESS

        def character(c_book, *args):
            if first[0]:
                first[0] = False
                eb.eb_write_text_byte2(c_book, 0xc6, 0xfc)
                return eb.EB_SUCCESS
            return eb.eb_hook_euc_to_ascii(c_book, *args)

        eb.eb_set_hooks(hooks, ((eb.EB_HOOK_BEGIN_NARROW, begin),
                               (eb.EB_HOOK_NARROW_JISX0208, character)))
        chunks = self.chunks(book, heading, eb.eb_read_heading, hooks)
        self.assertGreaterEqual(len(chunks), 2)
        self.assertEqual(chunks[0], b"x" * 2046)
        self.assertTrue(chunks[1].startswith(b"\xc6\xfc"))
        self.assertEqual(b"".join(chunks).decode("euc_jp"), "x" * 2046 + "日okyo")

    def test_forward_text_uses_the_owned_library_signature(self):
        book = self.open_book()
        _, position = self.find_tokyo(book)
        eb.eb_seek_text(book, position)
        eb.eb_forward_text(book, eb.EB_Appendix())
        self.assertGreater(eb.eb_tell_text(book), position)

    def test_rawtext_and_binary_read_preserve_the_fixture_bytes(self):
        source = (self.root / "epwing/chimei/DATA/HONMON").read_bytes()
        for name in ("epwing", "epwing-zipped"):
            with self.subTest(book=name):
                book = self.open_book(name)
                eb.eb_seek_text(book, (2, 0))
                raw = eb.eb_read_rawtext(book, 32)
                self.assertEqual(raw, source[2048:2080])
                self.assertIn(b"\0", raw)
                # A bounded 8x1 mono selection reads one authentic HONMON byte.
                # This exercises the native BMP reader; the fixture has no authored picture.
                eb.eb_set_binary_mono_graphic(book, (2, 0), 8, 1)
                parts = []
                for _ in range(100):
                    part = eb.eb_read_binary(book)
                    self.assertIsInstance(part, bytes)
                    if not part:
                        break
                    parts.append(part)
                else:
                    self.fail("binary read did not terminate")
                image = b"".join(parts)
                self.assertEqual(image[:2], b"BM")
                self.assertEqual(int.from_bytes(image[2:6], "little"), len(image))
                self.assertEqual(image[62:63], source[2048:2049])
                eb.eb_unset_binary(book)
                with self.assertRaises(eb.EBError):
                    eb.eb_read_binary(book)

    def test_real_gaiji_font_bitmaps_and_conversions(self):
        book = eb.EB_Book()
        eb.eb_bind(book, str(self.root / "epwing-gaiji"))
        eb.eb_set_subbook(book, eb.eb_subbook_list(book)[0])
        self.assertIn(eb.EB_FONT_16, eb.eb_font_list(book))
        eb.eb_set_font(book, eb.EB_FONT_16)
        self.assertEqual(eb.eb_font_height(book), 16)
        for width in ("narrow", "wide"):
            with self.subTest(width=width):
                self.assertTrue(getattr(eb, "eb_have_%s_font" % width)(book))
                start = getattr(eb, "eb_%s_font_start" % width)(book)
                end = getattr(eb, "eb_%s_font_end" % width)(book)
                self.assertLessEqual(start, end)
                bitmap = getattr(eb, "eb_%s_font_character_bitmap" % width)(book, start)
                size = getattr(eb, "eb_%s_font_size" % width)(book)
                self.assertEqual(len(bitmap), size)
                pixels = getattr(eb, "eb_%s_font_width" % width)(book)
                self.assertEqual(len(bitmap), ((pixels + 7) // 8) * 16)
                self.assertTrue(any(bitmap))
                for form, signature in (("xbm", b"#define"), ("xpm", b"/* XPM */"), ("gif", b"GIF89a")):
                    output = getattr(eb, "eb_bitmap_to_%s" % form)(bitmap, pixels, 16)
                    self.assertTrue(output.startswith(signature))
                    capacity = getattr(eb, "eb_%s_font_%s_size" % (width, form))(eb.EB_FONT_16)
                    self.assertLessEqual(len(output), capacity)

    def test_real_appendix_binding(self):
        appendix = eb.EB_Appendix()
        eb.eb_bind_appendix(appendix, str(self.root / "epwing-zipped/appendix"))
        subbooks = eb.eb_appendix_subbook_list(appendix)
        self.assertTrue(subbooks)
        eb.eb_set_appendix_subbook(appendix, subbooks[0])
        self.assertEqual(eb.eb_appendix_subbook(appendix), subbooks[0])


if __name__ == "__main__":
    unittest.main()
