import gc
import weakref
import unittest
from unittest import mock
import eb
import eblib


class PythonBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        eb.eb_initialize_library()

    @classmethod
    def tearDownClass(cls):
        eb.eb_finalize_library()

    def test_book_methods_are_real_bound_methods(self):
        book = eb.EB_Book()
        self.assertIsInstance(book.text_status(), int)
        self.assertIsInstance(book.auto_stop_code(), int)

    def test_search_queries_have_one_consistent_bytes_boundary(self):
        book = eb.EB_Book()
        for search in (eb.eb_search_exactword, eb.eb_search_word, eb.eb_search_endword):
            with self.subTest(search=search.__name__):
                with self.assertRaises(TypeError):
                    search(book, "日本")
                with self.assertRaises(eb.EBError):
                    search(book, "日本".encode("euc_jp"))

    def test_bitmap_output_is_binary_and_inputs_are_bounded(self):
        for convert in (eb.eb_bitmap_to_xbm, eb.eb_bitmap_to_xpm, eb.eb_bitmap_to_gif):
            with self.subTest(convert=convert.__name__):
                self.assertIsInstance(convert(b"\xff" * 8, 8, 8), bytes)
                with self.assertRaises(TypeError):
                    convert("not bytes", 8, 8)
                with self.assertRaises(ValueError):
                    convert(b"", 8, 8)
                with self.assertRaises(ValueError):
                    convert(b"\0" * 100, 10000, 10000)

    def test_read_binary_accepts_exactly_the_documented_book_argument(self):
        with self.assertRaises(eb.EBError):
            eb.eb_read_binary(eb.EB_Book())

    def test_high_level_content_joins_binary_chunks(self):
        # Exercise the public convenience result without a dictionary fixture.
        fake = eblib.EB.__new__(eblib.EB)
        fake.book = object()
        fake.appendix = object()
        fake.hookset = object()
        fake.subbook = lambda: 0
        original_seek = eblib.eb_seek_text
        eblib.eb_seek_text = lambda book, position: None
        chunks = iter([b"\xc6", b"\xfc", b""])
        try:
            result = fake.get_content(0, (1, 0), None, lambda *args: next(chunks))
            self.assertEqual(result, b"\xc6\xfc")
            self.assertEqual(result.decode("euc_jp"), "日")
        finally:
            eblib.eb_seek_text = original_seek

    def test_rawtext_lengths_and_forward_text_arguments(self):
        book, appendix = eb.EB_Book(), eb.EB_Appendix()
        with self.assertRaises(ValueError):
            eb.eb_read_rawtext(book, -1)
        with self.assertRaises(OverflowError):
            eb.eb_read_rawtext(book, 2 ** 63)
        with self.assertRaises(eb.EBError):
            eb.eb_read_rawtext(book, 8)
        with self.assertRaises(TypeError):
            eb.eb_forward_text(book, object())
        with self.assertRaises(eb.EBError):
            eb.eb_forward_text(book, appendix)

    def test_keywords_reject_unicode_and_nuls(self):
        book = eb.EB_Book()
        with self.assertRaises(TypeError):
            eb.eb_search_keyword(book, ("Tokyo",))
        with self.assertRaises(ValueError):
            eb.eb_search_keyword(book, (b"Tokyo\0other",))
        with self.assertRaises(eb.EBError):
            eb.eb_search_keyword(book, (b"Tokyo",))
        for search in (eb.eb_search_exactword, eb.eb_search_word, eb.eb_search_endword):
            with self.assertRaises(ValueError):
                search(book, b"Tokyo\0other")

    def test_predefined_hooks_bound_argument_vectors(self):
        args = (eb.EB_Book(), eb.EB_Appendix(), None, eb.EB_HOOK_STOP_CODE)
        with self.assertRaises(ValueError):
            eb.eb_hook_stop_code(*args, ())
        with self.assertRaises(ValueError):
            eb.eb_hook_stop_code(*args, (0,) * 7)
        with self.assertRaises(OverflowError):
            eb.eb_hook_stop_code(*args, (0, 2 ** 64))
        with self.assertRaises(OverflowError):
            eb.eb_hook_stop_code(*args, (0, -1))
        self.assertIsInstance(eb.eb_hook_stop_code(*args, (0, 0)), int)

    def test_hook_replacement_releases_callbacks_and_cycles(self):
        class Callback:
            def __call__(self, *args):
                return eb.EB_SUCCESS

        hooks = eb.EB_Hookset()
        callback = Callback()
        reference = weakref.ref(callback)
        eb.eb_set_hook(hooks, (eb.EB_HOOK_NEWLINE, callback))
        del callback
        self.assertIsNotNone(reference())
        eb.eb_set_hook(hooks, (eb.EB_HOOK_NEWLINE, lambda *args: eb.EB_SUCCESS))
        gc.collect()
        self.assertIsNone(reference())
        callback = Callback()
        callback.hooks = hooks
        reference = weakref.ref(callback)
        eb.eb_set_hooks(hooks, ((eb.EB_HOOK_NEWLINE, callback),))
        del callback, hooks
        gc.collect()
        self.assertIsNone(reference())

    def test_hook_batch_validation_keeps_previous_registration(self):
        hooks = eb.EB_Hookset()
        callback = lambda *args: eb.EB_SUCCESS
        for invalid in ("not a code", 2 ** 100, -1, 100000):
            with self.subTest(code=invalid):
                with self.assertRaises((TypeError, OverflowError, eb.EBError)):
                    eb.eb_set_hooks(hooks, ((eb.EB_HOOK_NEWLINE, callback),
                                            (invalid, callback)))
        with self.assertRaises(TypeError):
            eb.eb_set_hook(hooks, (eb.EB_HOOK_NEWLINE, None))

    def test_font_conversion_sizes_match_the_public_constants(self):
        for width in ("narrow", "wide"):
            for form in ("xbm", "xpm", "gif"):
                size = getattr(eb, "eb_%s_font_%s_size" % (width, form))
                for height in (16, 24, 30, 48):
                    with self.subTest(width=width, form=form, height=height):
                        font = getattr(eb, "EB_FONT_%d" % height)
                        capacity = size(font)
                        self.assertEqual(capacity, getattr(eb, "EB_SIZE_%s_FONT_%d_%s" %
                            (width.upper(), height, form.upper())))
                        pixels = getattr(eb, "EB_WIDTH_%s_FONT_%d" % (width.upper(), height))
                        bitmap = b"\xff" * (((pixels + 7) // 8) * height)
                        output = getattr(eb, "eb_bitmap_to_%s" % form)(bitmap, pixels, height)
                        self.assertEqual(len(output), capacity)
                with self.assertRaises(eb.EBError):
                    size(999)

    def test_get_content_restores_subbook_when_reading_raises(self):
        class ReadFailure(Exception):
            pass

        fake = eblib.EB.__new__(eblib.EB)
        fake.book, fake.appendix, fake.hookset = object(), object(), object()
        selected = [0]
        fake.subbook = lambda: selected[0]
        fake.set_subbook = lambda value: selected.__setitem__(0, value)
        with mock.patch.object(eblib, "eb_seek_text", side_effect=ReadFailure):
            with self.assertRaises(ReadFailure):
                fake.get_content(1, (1, 0), None, lambda *args: b"")
        self.assertEqual(selected[0], 0)


if __name__ == "__main__":
    unittest.main()
