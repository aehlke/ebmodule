/*
 *  ebmodule.c - EB library wapper module for Python
 *               Tamito KAJIYAMA <2 February 2001>
 *
 *               Modified (2.4) by Alex Ehlke, July 28, 2010:
 *                 Windows compatibility.
 *               Modified (2.2) by Alex Ehlke, June 6, 2010:
 *                 Added preliminary support for EB-4.3, and 
 *                 eb_is_text_stopped function
 */

static char *version = ""
"$Id: ebmodule.c,v 2.4 2001/09/22 23:42:05 kajiyama Exp $";

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include <limits.h>

/* Python 3: EB-encoded text and binary buffers are bytes, never implicit UTF-8. */
#define PyInt_FromLong PyLong_FromLong
#define PyInt_Check PyLong_Check
#define PyInt_AsLong PyLong_AsLong
#define PyString_FromStringAndSize PyBytes_FromStringAndSize
#define PyString_FromString PyBytes_FromString
#define PyString_Check PyBytes_Check
#define PyString_AsString PyBytes_AsString

#include "eb/eb.h"
#include "eb/appendix.h"
#include "eb/text.h"
#include "eb/error.h"
#include "eb/font.h"
#include "eb/binary.h"

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif

/* global variables */
static PyObject *EBError;
static PyObject *CallbackContext;

static void
set_eb_error(int status)
{
  PyObject *value = Py_BuildValue("(is)", status, eb_error_message(status));
  if (value != NULL) {
    PyErr_SetObject(EBError, value);
    Py_DECREF(value);
  }
}

#define DEFERRED_ADDRESS(ADDR) 0

/*********************************************************************
 *  Book object
 */

typedef struct {
  PyObject_HEAD
  EB_Book book;
} BookObject;

/*static PyObject * Book_name(BookObject *self)*/
/*{*/
    /*PyObject *args, *result;*/
    /*result = PyString_FromString("hello world!");*/
    /*[>Py_DECREF(args);<]*/
    
    /*return result;*/
/*}*/

static PyObject *Book_text_status(BookObject *self, PyObject *unused)
{
    PyObject *result;
#if EB_VERSION_MAJOR < 4
    result = PyInt_FromLong(self->book.text_context.code);
#else
    result = PyInt_FromLong(self->book.text_context.text_status);
#endif
    return result;
}
static PyObject *Book_auto_stop_code(BookObject *self, PyObject *unused)
{
    return PyInt_FromLong(self->book.text_context.auto_stop_code);
}

static struct PyMethodDef Book_methods[] = {
	/*{(char*)"helloworld", T_OBJECT_EX, offsetof(BookObject, book), 0, (char*)"blah"},*/
/*{"name", (PyCFunction)Book_name, METH_NOARGS,*/
     /*"Return the name, combining the first and last name"*/
    /*},    */
    {"text_status", (PyCFunction)Book_text_status, METH_NOARGS, "Return the native text status (content mode on EB 3.x)."},
    {"auto_stop_code", (PyCFunction)Book_auto_stop_code, METH_NOARGS, "Return the book context's auto stop code."},
  {NULL, NULL}  /* sentinel */
};

static PyTypeObject BookType;

static BookObject *
book_new(void)
{
  BookObject *self;
	
  self = PyObject_NEW(BookObject, &BookType);
  if (self == NULL)
    return NULL;
  eb_initialize_book(&self->book);
  return self;
}

static void
book_dealloc(BookObject *self)
{
  eb_finalize_book(&self->book);
  PyObject_Del(self);
}


/*static PyMemberDef Book_members[] = {*/
	/*{(char*)"helloworld", T_OBJECT_EX, offsetof(BookObject, book), 0, (char*)"blah"},*/
	/*{NULL}  [> Sentinel <]*/
/*};*/

static char BookType__doc__[] =
"A data structure for representing Book objects.";

static PyTypeObject BookType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "eb.EB_Book",
  .tp_basicsize = sizeof(BookObject),
  .tp_dealloc = (destructor)book_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = Book_methods,
  .tp_doc = BookType__doc__,
};

/*********************************************************************
 *  Appendix object
 */

typedef struct {
  PyObject_HEAD
  EB_Appendix appendix;
} AppendixObject;

static struct PyMethodDef Appendix_methods[] = {
  {NULL, NULL}  /* sentinel */
};

static PyTypeObject AppendixType;

static AppendixObject *
appendix_new(void)
{
  AppendixObject *self;
	
  self = PyObject_NEW(AppendixObject, &AppendixType);
  if (self == NULL)
    return NULL;
  eb_initialize_appendix(&self->appendix);
  return self;
}

static void
appendix_dealloc(AppendixObject *self)
{
  eb_finalize_appendix(&self->appendix);
  PyObject_Del(self);
}


static char AppendixType__doc__[] =
"A data structure for representing Appendix objects.";

static PyTypeObject AppendixType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "eb.EB_Appendix",
  .tp_basicsize = sizeof(AppendixObject),
  .tp_dealloc = (destructor)appendix_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = Appendix_methods,
  .tp_doc = AppendixType__doc__,
};

/*********************************************************************
 *  Hookset object
 */

typedef struct {
  PyObject_HEAD
  EB_Hookset hookset;
  PyObject *callbacks;
} HooksetObject;

static struct PyMethodDef Hookset_methods[] = {
  {NULL, NULL}  /* sentinel */
};

static PyTypeObject HooksetType;

static HooksetObject *
hookset_new(void)
{
  HooksetObject *self;
	
  self = PyObject_GC_New(HooksetObject, &HooksetType);
  if (self == NULL)
    return NULL;
  eb_initialize_hookset(&self->hookset);
  self->callbacks = PyDict_New();
  if (self->callbacks == NULL) {
    eb_finalize_hookset(&self->hookset);
    PyObject_GC_Del(self);
    return NULL;
  }
  PyObject_GC_Track(self);
  return self;
}

static int
hookset_traverse(HooksetObject *self, visitproc visit, void *arg)
{
  Py_VISIT(self->callbacks);
  return 0;
}

static int
hookset_clear(HooksetObject *self)
{
  Py_CLEAR(self->callbacks);
  return 0;
}

static void
hookset_dealloc(HooksetObject *self)
{
  PyObject_GC_UnTrack(self);
  eb_finalize_hookset(&self->hookset);
  hookset_clear(self);
  PyObject_GC_Del(self);
}


static char HooksetType__doc__[] =
"A data type for representing Hookset objects.";

static PyTypeObject HooksetType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "eb.EB_Hookset",
  .tp_basicsize = sizeof(HooksetObject),
  .tp_dealloc = (destructor)hookset_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
  .tp_traverse = (traverseproc)hookset_traverse,
  .tp_clear = (inquiry)hookset_clear,
  .tp_methods = Hookset_methods,
  .tp_doc = HooksetType__doc__,
};

/*********************************************************************
 *  Module functions
 */

/* [eb.h] */

/* eb.c */

static char py_eb_initialize_library__doc__[] = " eb_initialize_library() => None *Initialize the EB library.  Raise EBError if failed.";

static PyObject *
py_eb_initialize_library(PyObject *self, PyObject *args)
{
  int status;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  status = eb_initialize_library();
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_finalize_library__doc__[] = "eb_finalize_library() => None Finalize the EB library.";

static PyObject *
py_eb_finalize_library(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  eb_finalize_library();
  Py_INCREF(Py_None);
  return Py_None;
}

/* copyright.c */

static char py_eb_have_copyright__doc__[] = "eb_have_copyright(book) => int Return true if the current subbook has a copyright notice.";

static PyObject *
py_eb_have_copyright(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_copyright(&book->book));
}

static char py_eb_copyright__doc__[] = "eb_copyright(book) => (int, int) Return a position of the copyright notice.  Raise EBError if failed.";

static PyObject *
py_eb_copyright(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_copyright(&book->book, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return Py_BuildValue("(ii)", pos.page, pos.offset);
}

/* eb.c */

static char py_eb_Book__doc__[] = "EB_Book() => book Return a new instance of the Book object.";

static PyObject *
py_eb_Book(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  return (PyObject *)book_new();
}

static char py_eb_bind__doc__[] = "eb_bind(book, string) => None Bind the book object to an electronic book in the given path.  Raise EBError if failed.";

static PyObject *
py_eb_bind(PyObject *self, PyObject *args)
{
  BookObject *book;
  char *path;
  int status;

  if (!PyArg_ParseTuple(args, "O!s", &BookType, &book, &path))
    return NULL;
  status = eb_bind(&book->book, path);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_suspend__doc__[] = "eb_suspend(book) => None Suspend the use of the book.  The current subbook, language, and font are to be unselected.";

static PyObject *
py_eb_suspend(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  eb_suspend(&book->book);
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_is_bound__doc__[] = "eb_is_bound(book) => int Return true if the book object is bound to an electronic book.";

static PyObject *
py_eb_is_bound(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_is_bound(&book->book));
}

static char py_eb_path__doc__[] = "eb_path(book) => string Return the path of the electronic book.  Raise EBError if failed.";

static PyObject *
py_eb_path(PyObject *self, PyObject *args)
{
  BookObject *book;
  char path[PATH_MAX + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_path(&book->book, path);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(path);
}

static char py_eb_disc_type__doc__[] = "eb_disc_type(book) => int Return the disc type of the book.  Raise EBError if failed.";

static PyObject *
py_eb_disc_type(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Disc_Code disc_type;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_disc_type(&book->book, &disc_type);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(disc_type);
}

static char py_eb_character_code__doc__[] = "eb_character_code(book) => int Return the character code of the book.  Raise EBError if failed.";

static PyObject *
py_eb_character_code(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Character_Code char_code;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_character_code(&book->book, &char_code);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(char_code);
}

/* menu.c */

static char py_eb_have_menu__doc__[] = "eb_have_menu(book) => int Return true if the book has a menu.";

static PyObject *
py_eb_have_menu(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_menu(&book->book));
}

static char py_eb_menu__doc__[] = "eb_menu(book) => (int, int) Return the position of the book's menu.  Raise EBError if failed.";

static PyObject *
py_eb_menu(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_menu(&book->book, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return Py_BuildValue("(ii)", pos.page, pos.offset);
}

/* exactword.c */

static char py_eb_have_exactword_search__doc__[] = "eb_have_exactword_search(book) => int Return true if the book has the \"exact word\" search facility.";

static PyObject *
py_eb_have_exactword_search(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_exactword_search(&book->book));
}

static char py_eb_search_exactword__doc__[] = "eb_search_exactword(book, string) => None Issue an \"exact word\" search.  Raise EBError if failed.";

static PyObject *
py_eb_search_exactword(PyObject *self, PyObject *args)
{
  BookObject *book;
  const char *query;
  int status;

  if (!PyArg_ParseTuple(args, "O!y", &BookType, &book, &query))
    return NULL;
  status = eb_search_exactword(&book->book, query);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

/* word.c */

static char py_eb_have_word_search__doc__[] = "eb_have_word_search(book) => int Return true if the book has the word search facility.";

static PyObject *
py_eb_have_word_search(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_word_search(&book->book));
}

static char py_eb_search_word__doc__[] = "eb_search_word(book, string) => None Issue a word search.  Raise EBError if failed.";

static PyObject *
py_eb_search_word(PyObject *self, PyObject *args)
{
  BookObject *book;
  const char *query;
  int status;

  if (!PyArg_ParseTuple(args, "O!y", &BookType, &book, &query))
    return NULL;
  status = eb_search_word(&book->book, query);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

/* endword.c */

static char py_eb_have_endword_search__doc__[] = "eb_have_endword_search(book) => int Return true if the book has the \"end word\" search facility.";

static PyObject *
py_eb_have_endword_search(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_endword_search(&book->book));
}

static char py_eb_search_endword__doc__[] = "eb_search_endword(book, string) => None Issue an \"end word\" search.  Raise EBError if failed.";

static PyObject *
py_eb_search_endword(PyObject *self, PyObject *args)
{
  BookObject *book;
  const char *query;
  int status;

  if (!PyArg_ParseTuple(args, "O!y", &BookType, &book, &query))
    return NULL;
  status = eb_search_endword(&book->book, query);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

/* keyword.c */

static char py_eb_have_keyword_search__doc__[] = "eb_have_keyword_search(book) => int Return true if the book has the keyword search facility.";

static PyObject *
py_eb_have_keyword_search(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_keyword_search(&book->book));
}

static char py_eb_search_keyword__doc__[] = "eb_search_keyword(book, string[]) => None Issue a keyword search.  Raise EBError if failed.";

static PyObject *
py_eb_search_keyword(PyObject *self, PyObject *args)
{
  BookObject *book;
  PyObject *argv, *element;
  char *keywords[EB_MAX_KEYWORDS+1];
  Py_ssize_t argc, i, length;
  int status;

  if (!PyArg_ParseTuple(args, "O!O!", &BookType, &book, &PyTuple_Type, &argv))
    return NULL;
  if ((argc = PyTuple_Size(argv)) > EB_MAX_KEYWORDS) {
    PyErr_SetString(PyExc_ValueError, "Too much keywords");
    return NULL;
  }
  for (i = 0; i < argc; i++) {
    element = PyTuple_GetItem(argv, i);
    if (!PyBytes_Check(element)) {
      PyErr_SetString(PyExc_TypeError, "2nd argument must be a tuple of bytes");
      return NULL;
    }
    if (PyBytes_AsStringAndSize(element, &keywords[i], &length) < 0)
      return NULL;
    if (memchr(keywords[i], '\0', (size_t)length) != NULL) {
      PyErr_SetString(PyExc_ValueError, "keyword contains an embedded NUL");
      return NULL;
    }
  }
  keywords[argc] = NULL;
  status = eb_search_keyword(&book->book, (const char * const *)keywords);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

/* multi.c */

static char py_eb_have_multi_search__doc__[] = "eb_have_multi_search(book) => int Return true if the book has the multiple search facility.";

static PyObject *
py_eb_have_multi_search(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_multi_search(&book->book));
}

static char py_eb_multi_search_list__doc__[] = "eb_multi_search_list(book) => int[] Return a tuple of multi search codes in the book.  Raise EBError if failed.";

static PyObject *
py_eb_multi_search_list(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Multi_Search_Code multilist[EB_MAX_MULTI_SEARCHES];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_multi_search_list(&book->book, multilist, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, PyInt_FromLong(multilist[i]));
  return tuple;
}

static char py_eb_multi_entry_list__doc__[] = "eb_multi_entry_list(book, int) => int[] Return a tuple of entries of the multi search.  Raise EBError if failed.";

static PyObject *
py_eb_multi_entry_list(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Multi_Search_Code search;
  EB_Multi_Entry_Code entrylist[EB_MAX_MULTI_ENTRIES];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &search))
    return NULL;
  status = eb_multi_entry_list(&book->book, search, entrylist, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, PyInt_FromLong(entrylist[i]));
  return tuple;
}

static char py_eb_multi_entry_label__doc__[] = "eb_multi_entry_label(book, int, int) => int[] Return the label of the entry of the multi search.  Raise EBError if failed.";

static PyObject *
py_eb_multi_entry_label(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Multi_Search_Code search;
  EB_Multi_Entry_Code entry;
  char buffer[EB_MAX_MULTI_LABEL_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &search, &entry))
    return NULL;
  status = eb_multi_entry_label(&book->book, search, entry, buffer);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(buffer);
}

static char py_eb_multi_entry_have_candidates__doc__[] = "eb_multi_entry_have_candidates(book, int, int) => int Return true if the entry of the multi search has candidates.";

static PyObject *
py_eb_multi_entry_have_candidates(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Multi_Search_Code search;
  EB_Multi_Entry_Code entry;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &search, &entry))
    return NULL;
  return PyInt_FromLong(eb_multi_entry_have_candidates(&book->book, search, entry));
}

static char py_eb_multi_entry_candidates__doc__[] = "eb_multi_entry_candidates(book, int, int) => int Return the position of the candidates of the entry.  Raise EBError if failed.";

static PyObject *
py_eb_multi_entry_candidates(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Multi_Search_Code search;
  EB_Multi_Entry_Code entry;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &search, &entry))
    return NULL;
  status = eb_multi_entry_candidates(&book->book, search, entry, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return Py_BuildValue("(ii)", pos.page, pos.offset);
}

/* search.c */

#define MAX_HITS 50

static char py_eb_hit_list__doc__[] = "eb_hit_list(book) => ((int, int) (int, int))[] Return a tuple of hits obtained by the last search.  Each hit is represented by a pair of positions for the heading and body of a section.  The number of hits returned by a call is limited to 50, so that an application program have to call the function until an empty tuple is returned in order to get all hits.  Raise EBError if failed.";

static PyObject *
py_eb_hit_list(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Hit hits[MAX_HITS];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_hit_list(&book->book, MAX_HITS, hits, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, Py_BuildValue("((ii)(ii))",
					    hits[i].heading.page,
					    hits[i].heading.offset,
					    hits[i].text.page,
					    hits[i].text.offset));
  return tuple;
}

/* subbook.c */

static char py_eb_load_all_subbooks__doc__[] = "eb_load_all_subbooks(book) => None Load all subbook in the book.  Raise EBError if failed.";

static PyObject *
py_eb_load_all_subbooks(PyObject *self, PyObject *args)
{
  BookObject *book;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_load_all_subbooks(&book->book);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_subbook_list__doc__[] = "eb_subbook_list(book) => int[] Return a tuple of subbook codes in the book.  Raise EBError if failed.";

static PyObject *
py_eb_subbook_list(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Subbook_Code sublist[EB_MAX_SUBBOOKS];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_subbook_list(&book->book, sublist, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, PyInt_FromLong(sublist[i]));
  return tuple;
}

static char py_eb_subbook__doc__[] = "eb_subbook(book) => int Return the current subbook's code.  Raise EBError if failed.";

static PyObject *
py_eb_subbook(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Subbook_Code subbook;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_subbook(&book->book, &subbook);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(subbook);
}

static char py_eb_subbook_title__doc__[] = "eb_subbook_title(book) => string Return the title of the current subbook.  Raise EBError if failed.";

static PyObject *
py_eb_subbook_title(PyObject *self, PyObject *args)
{
  BookObject *book;
  char title[EB_MAX_TITLE_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_subbook_title(&book->book, title);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(title);
}

static char py_eb_subbook_title2__doc__[] = "eb_subbook_title2(book, int) => string Return the title of the subbook specified with the given subbook code.  Raise EBError if failed.";

static PyObject *
py_eb_subbook_title2(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Subbook_Code subbook;
  char title[EB_MAX_TITLE_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &subbook))
    return NULL;
  status = eb_subbook_title2(&book->book, subbook, title);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(title);
}

static char py_eb_subbook_directory__doc__[] = "eb_subbook_directory(book) => string Return the directory of the current subbook.  Raise EBError if failed.";

static PyObject *
py_eb_subbook_directory(PyObject *self, PyObject *args)
{
  BookObject *book;
  char directory[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_subbook_directory(&book->book, directory);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(directory);
}

static char py_eb_subbook_directory2__doc__[] = "eb_subbook_directory2(book, int) => string Return the directory of the subbook specified with the given subbook code.  Raise EBError if failed.";

static PyObject *
py_eb_subbook_directory2(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Subbook_Code subbook;
  char directory[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &subbook))
    return NULL;
  status = eb_subbook_directory2(&book->book, subbook, directory);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(directory);
}

static char py_eb_set_subbook__doc__[] = "eb_set_subbook(book, int) => None Specify the current subbook.  Raise EBError if failed.";

static PyObject *
py_eb_set_subbook(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Subbook_Code subbook;
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &subbook))
    return NULL;
  status = eb_set_subbook(&book->book, subbook);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_unset_subbook__doc__[] = "eb_unset_subbook(book) => None Unselect the current subbook.";

static PyObject *
py_eb_unset_subbook(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  eb_unset_subbook(&book->book);
  Py_INCREF(Py_None);
  return Py_None;
}

/* lock.c */

static char py_eb_pthread_enabled__doc__[] = "eb_pthread_enabled() => int Return true if the pthread support is enabled.";

static PyObject *
py_eb_pthread_enabled(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  return PyInt_FromLong(eb_pthread_enabled());
}

/* log.c */

/* [appendix.h] */

/* appendix.c */

static char py_eb_Appendix__doc__[] = "EB_Appendix() => appendix Return a new instance of the Appendix object.";

static PyObject *
py_eb_Appendix(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  return (PyObject *)appendix_new();
}

static char py_eb_suspend_appendix__doc__[] = "eb_suspend_appendix(appendix) => None Suspend the use of the appendix.";

static PyObject *
py_eb_suspend_appendix(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  eb_suspend_appendix(&appendix->appendix);
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_bind_appendix__doc__[] = "eb_bind_appendix(appendix, string) => None Bind the appendix object to an appendix in the given path.  Raise EBError if failed.";

static PyObject *
py_eb_bind_appendix(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  char *path;
  int status;

  if (!PyArg_ParseTuple(args, "O!s", &AppendixType, &appendix, &path))
    return NULL;
  status = eb_bind_appendix(&appendix->appendix, path);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_is_appendix_bound__doc__[] = "eb_is_appendix_bound(appendix) => int Return true if the appendix is bound.";

static PyObject *
py_eb_is_appendix_bound(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  return PyInt_FromLong(eb_is_appendix_bound(&appendix->appendix));
}

static char py_eb_appendix_path__doc__[] = "eb_appendix_path(appendix) => string Return the path of the appendix.  Raise EBError if failed.";

static PyObject *
py_eb_appendix_path(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  char path[PATH_MAX + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_appendix_path(&appendix->appendix, path);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(path);
}

/* appsub.c */

static char py_eb_load_all_appendix_subbooks__doc__[] = "eb_load_all_appendix_subbooks(appendix) => None Load all subbook in the appendix.  Raise EBError if failed.";

static PyObject *
py_eb_load_all_appendix_subbooks(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_load_all_appendix_subbooks(&appendix->appendix);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_appendix_subbook_list__doc__[] = "eb_appendix_subbook_list(appendix) => int[] Return a tuple of subbook codes in the appendix.  Raise EBError if failed.";

static PyObject *
py_eb_appendix_subbook_list(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  EB_Subbook_Code sublist[EB_MAX_SUBBOOKS];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_appendix_subbook_list(&appendix->appendix, sublist, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, PyInt_FromLong(sublist[i]));
  return tuple;
}

static char py_eb_appendix_subbook__doc__[] = "eb_appendix_subbook(appendix) => int Return the current subbook's code.  Raise EBError if failed.";

static PyObject *
py_eb_appendix_subbook(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  EB_Subbook_Code subbook;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_appendix_subbook(&appendix->appendix, &subbook);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(subbook);
}

static char py_eb_appendix_subbook_directory__doc__[] = "eb_appendix_subbook_directory(appendix) => string Return the directory of the current subbook.  Raise EBError if failed.";

static PyObject *
py_eb_appendix_subbook_directory(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  char directory[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_appendix_subbook_directory(&appendix->appendix, directory);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(directory);
}

static char py_eb_appendix_subbook_directory2__doc__[] = "eb_appendix_subbook_directory2(appendix, int) => string Return the directory of the subbook specified with the given subbook code.  Raise EBError if failed.";

static PyObject *
py_eb_appendix_subbook_directory2(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  EB_Subbook_Code subbook;
  char directory[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &AppendixType, &appendix, &subbook))
    return NULL;
  status = eb_appendix_subbook_directory2(&appendix->appendix, subbook,
                                          directory);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(directory);
}

static char py_eb_set_appendix_subbook__doc__[] = "eb_set_appendix_subbook(appendix, int) => None Specify the current subbook of the appendix.  Raise EBError if failed.";

static PyObject *
py_eb_set_appendix_subbook(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  EB_Subbook_Code subbook;
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &AppendixType, &appendix, &subbook))
    return NULL;
  status = eb_set_appendix_subbook(&appendix->appendix, subbook);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_unset_appendix_subbook__doc__[] = "eb_unset_appendix_subbook(appendix) => None Unselect the current subbook of the appendix.";

static PyObject *
py_eb_unset_appendix_subbook(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  eb_unset_appendix_subbook(&appendix->appendix);
  Py_INCREF(Py_None);
  return Py_None;
}

/* stopcode.c */

static char py_eb_have_stop_code__doc__[] = "eb_have_stop_code(appendix) => int Return true if the appendix has the stop code.";

static PyObject *
py_eb_have_stop_code(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  return PyInt_FromLong(eb_have_stop_code(&appendix->appendix));
}

static char py_eb_stop_code__doc__[] = "eb_stop_code(appendix) => int Return the stop code in the appendix.  Raise EBError if failed.";

static PyObject *
py_eb_stop_code(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status, stop_code;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_stop_code(&appendix->appendix, &stop_code);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(stop_code);
}

/* narwalt.c */

static char py_eb_have_narrow_alt__doc__[] = "eb_have_narrow_alt(appendix) => int Return true if the appendix has alternation strings for the narrow font(s).";

static PyObject *
py_eb_have_narrow_alt(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  return PyInt_FromLong(eb_have_narrow_alt(&appendix->appendix));
}

static char py_eb_narrow_alt_start__doc__[] = "eb_narrow_alt_start(appendix) => int Return the first character code of alternation strings for the narrow font(s).  Raise EBError if failed.";

static PyObject *
py_eb_narrow_alt_start(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status, start;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_narrow_alt_start(&appendix->appendix, &start);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(start);
}

static char py_eb_narrow_alt_end__doc__[] = "eb_narrow_alt_end(appendix) => int Return the last character code of alternation strings for the narrow font(s).  Raise EBError if failed.";

static PyObject *
py_eb_narrow_alt_end(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status, end;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_narrow_alt_end(&appendix->appendix, &end);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(end);
}

static char py_eb_narrow_alt_character_text__doc__[] = "eb_narrow_alt_character_text(appendix, int) => string Return the alternation string for the specified character code.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_alt_character_text(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int ch, status;
  char buffer[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];

  if (!PyArg_ParseTuple(args, "O!i", &AppendixType, &appendix, &ch))
    return NULL;
  status = eb_narrow_alt_character_text(&appendix->appendix, ch, buffer);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(buffer);
}

static char py_eb_forward_narrow_alt_character__doc__[] = ""; static PyObject * py_eb_forward_narrow_alt_character(PyObject *self, PyObject *args) {
  AppendixObject *appendix;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &AppendixType, &appendix, &n, &ch))
    return NULL;
  status = eb_forward_narrow_alt_character(&appendix->appendix, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

static char py_eb_backward_narrow_alt_character__doc__[] = ""; static PyObject * py_eb_backward_narrow_alt_character(PyObject *self, PyObject *args) {
  AppendixObject *appendix;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &AppendixType, &appendix, &n, &ch))
    return NULL;
  status = eb_backward_narrow_alt_character(&appendix->appendix, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

/* widealt.c */

static char py_eb_have_wide_alt__doc__[] = "eb_have_wide_alt(appendix) => int Return true if the appendix has alternation strings for the wide font(s).";

static PyObject *
py_eb_have_wide_alt(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  return PyInt_FromLong(eb_have_wide_alt(&appendix->appendix));
}

static char py_eb_wide_alt_start__doc__[] = "eb_wide_alt_start(appendix) => int Return the first character code of alternation strings for the wide font(s).  Raise EBError if failed.";

static PyObject *
py_eb_wide_alt_start(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status, start;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_wide_alt_start(&appendix->appendix, &start);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(start);
}

static char py_eb_wide_alt_end__doc__[] = "eb_wide_alt_end(appendix) => int Return the last character code of alternation strings for the wide font(s).  Raise EBError if failed.";

static PyObject *
py_eb_wide_alt_end(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int status, end;

  if (!PyArg_ParseTuple(args, "O!", &AppendixType, &appendix))
    return NULL;
  status = eb_wide_alt_end(&appendix->appendix, &end);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(end);
}

static char py_eb_wide_alt_character_text__doc__[] = "eb_wide_alt_character_text(appendix, int) => string Return the alternation string for the specified character code.  Raise EBError if failed.";

static PyObject *
py_eb_wide_alt_character_text(PyObject *self, PyObject *args)
{
  AppendixObject *appendix;
  int ch, status;
  char buffer[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];

  if (!PyArg_ParseTuple(args, "O!i", &AppendixType, &appendix, &ch))
    return NULL;
  status = eb_wide_alt_character_text(&appendix->appendix, ch, buffer);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromString(buffer);
}

static char py_eb_forward_wide_alt_character__doc__[] = ""; static PyObject * py_eb_forward_wide_alt_character(PyObject *self, PyObject *args) {
  AppendixObject *appendix;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &AppendixType, &appendix, &n, &ch))
    return NULL;
  status = eb_forward_wide_alt_character(&appendix->appendix, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

static char py_eb_backward_wide_alt_character__doc__[] = ""; static PyObject * py_eb_backward_wide_alt_character(PyObject *self, PyObject *args) {
  AppendixObject *appendix;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &AppendixType, &appendix, &n, &ch))
    return NULL;
  status = eb_backward_wide_alt_character(&appendix->appendix, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

/* [text.h] */

/* hook.c */

#define EB_MAX_ARGV 6  /* eb-3.1 (text.c) */

static PyObject *
call_predefined_hook(
     EB_Error_Code (*func)(EB_Book *, EB_Appendix *, void *,
                           EB_Hook_Code, int, const unsigned int *),
     PyObject *args)
{
  BookObject *book;
  AppendixObject *appendix;
  PyObject *container, *argv, *element;
  EB_Hook_Code code;
  int i, c_argc;
  unsigned int c_argv[EB_MAX_ARGV];

  if (!PyArg_ParseTuple(args, "O!O!OiO!",
                        &BookType, &book, &AppendixType, &appendix,
                        &container, &code, &PyTuple_Type, &argv))
    return NULL;
  if (PyTuple_GET_SIZE(argv) > EB_MAX_ARGV) {
    PyErr_SetString(PyExc_ValueError, "too many predefined hook arguments");
    return NULL;
  }
  c_argc = (int)PyTuple_GET_SIZE(argv);
  if ((func == eb_hook_stop_code && c_argc < 2) ||
      (func != eb_hook_newline && c_argc < 1)) {
    PyErr_SetString(PyExc_ValueError, "not enough predefined hook arguments");
    return NULL;
  }
  for (i = 0; i < c_argc; i++) {
    element = PyTuple_GetItem(argv, i);
    if (!PyInt_Check(element)) {
      PyErr_SetString(PyExc_ValueError,
                      "4th argument must be a tuple of integers");
      return NULL;
    }
    {
      unsigned long value = PyLong_AsUnsignedLong(element);
      if (PyErr_Occurred()) return NULL;
      if (value > UINT_MAX) {
        PyErr_SetString(PyExc_OverflowError, "hook argument is outside the C unsigned int range");
        return NULL;
      }
      c_argv[i] = (unsigned int)value;
    }
  }
  return PyInt_FromLong(func(&book->book, &appendix->appendix, container,
                             code, c_argc, c_argv));
}

static char py_eb_hook_euc_to_ascii__doc__[] = ""; static PyObject * py_eb_hook_euc_to_ascii(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_euc_to_ascii, args);
}

static char py_eb_hook_stop_code__doc__[] = "";
static PyObject *py_eb_hook_stop_code(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_stop_code, args);
}

static char py_eb_hook_narrow_character_text__doc__[] = ""; static PyObject * py_eb_hook_narrow_character_text(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_narrow_character_text, args);
}

static char py_eb_hook_wide_character_text__doc__[] = ""; static PyObject * py_eb_hook_wide_character_text(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_wide_character_text, args);
}

static char py_eb_hook_newline__doc__[] = ""; static PyObject * py_eb_hook_newline(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_newline, args);
}

/*static char py_eb_hook_empty__doc__[] = ""; static PyObject * py_eb_hook_empty(PyObject *self, PyObject *args) {
  return call_predefined_hook(eb_hook_empty, args);
}*/

/* text.c */

static char py_eb_Hookset__doc__[] = "EB_Hookset() => hookset Return a new instance of the Hookset object.";

static PyObject *
py_eb_Hookset(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  return (PyObject *)hookset_new();
}

static EB_Error_Code
hook_handler(EB_Book *c_book, EB_Appendix *c_appendix, void *container,
             EB_Hook_Code code, int c_argc, const unsigned int *c_argv)
{
  PyObject *key = NULL, *context = NULL, *argv = NULL, *callback;
  PyObject *hook_code = NULL, *result = NULL, *item;
  HooksetObject *hookset;
  long status = EB_ERR_FAIL_READ_TEXT;
  int i;

  if (PyErr_Occurred()) return EB_ERR_FAIL_READ_TEXT;
  key = PyLong_FromLong(c_book->code);
  if (key == NULL) goto done;
  context = PyDict_GetItemWithError(CallbackContext, key);
  if (context == NULL) {
    if (!PyErr_Occurred())
      PyErr_SetString(PyExc_RuntimeError, "missing EB callback context");
    goto done;
  }
  Py_INCREF(context);
  hookset = (HooksetObject *)PyTuple_GET_ITEM(context, 2);
  hook_code = PyLong_FromLong(code);
  if (hook_code == NULL) goto done;
  callback = PyDict_GetItemWithError(hookset->callbacks, hook_code);
  if (callback == NULL) {
    if (!PyErr_Occurred())
      PyErr_SetString(PyExc_RuntimeError, "missing EB hook callback");
    goto done;
  }
  argv = PyTuple_New(c_argc);
  if (argv == NULL) goto done;
  for (i = 0; i < c_argc; i++) {
    item = PyLong_FromUnsignedLong(c_argv[i]);
    if (item == NULL) goto done;
    PyTuple_SET_ITEM(argv, i, item);
  }
  /* Own the callable across callbacks that replace their own hook. */
  Py_INCREF(callback);
  result = PyObject_CallFunctionObjArgs(callback,
      PyTuple_GET_ITEM(context, 0), PyTuple_GET_ITEM(context, 1),
      (PyObject *)container, hook_code, argv, NULL);
  Py_DECREF(callback);
  if (result == NULL) goto done;
  if (!PyLong_Check(result)) {
    PyErr_SetString(PyExc_TypeError, "hook function must return an integer");
    goto done;
  }
  status = PyLong_AsLong(result);
  if (!PyErr_Occurred() && (status < INT_MIN || status > INT_MAX))
    PyErr_SetString(PyExc_OverflowError, "hook status is outside the C int range");
done:
  Py_XDECREF(result);
  Py_XDECREF(argv);
  Py_XDECREF(hook_code);
  Py_XDECREF(context);
  Py_XDECREF(key);
  return PyErr_Occurred() ? EB_ERR_FAIL_READ_TEXT : (int)status;
}

static char py_eb_set_hook__doc__[] = "eb_set_hook(hookset, (int, callable)) => None Register a hook function.  Raise EBError if failed.";

static PyObject *
py_eb_set_hooks(PyObject *self, PyObject *args);

static PyObject *
py_eb_set_hook(PyObject *self, PyObject *args)
{
  HooksetObject *hookset;
  PyObject *pair, *hooks, *call_args, *result;
  if (!PyArg_ParseTuple(args, "O!O", &HooksetType, &hookset, &pair))
    return NULL;
  hooks = PyTuple_Pack(1, pair);
  if (hooks == NULL) return NULL;
  call_args = PyTuple_Pack(2, hookset, hooks);
  Py_DECREF(hooks);
  if (call_args == NULL) return NULL;
  result = py_eb_set_hooks(self, call_args);
  Py_DECREF(call_args);
  return result;
}

static char py_eb_set_hooks__doc__[] = "eb_set_hooks(hookset, (int, callable)[]) => None Register a set of hook functions.";

static PyObject *
py_eb_set_hooks(PyObject *self, PyObject *args)
{
  HooksetObject *hookset;
  PyObject *tuple, *element, *key, *callbacks;
  EB_Hook hooks[EB_NUMBER_OF_HOOKS + 1];
  Py_ssize_t size, i;
  long code;
  int status;

  if (!PyArg_ParseTuple(args, "O!O!",
                        &HooksetType, &hookset, &PyTuple_Type, &tuple))
    return NULL;
  size = PyTuple_GET_SIZE(tuple);
  if (size > EB_NUMBER_OF_HOOKS) {
    PyErr_SetString(PyExc_ValueError, "too many hooks");
    return NULL;
  }
  callbacks = PyDict_Copy(hookset->callbacks);
  if (callbacks == NULL) return NULL;
  /* Validate the whole batch and allocate its dictionary before changing C hooks. */
  for (i = 0; i < size; i++) {
    element = PyTuple_GET_ITEM(tuple, i);
    if (!PyTuple_Check(element) || PyTuple_GET_SIZE(element) != 2) {
      PyErr_SetString(PyExc_TypeError, "each hook must be a pair of a hook code and function");
      goto error;
    }
    key = PyTuple_GET_ITEM(element, 0);
    code = PyLong_AsLong(key);
    if (PyErr_Occurred()) goto error;
    if (code < 0 || code >= EB_NUMBER_OF_HOOKS) {
      set_eb_error(EB_ERR_NO_SUCH_HOOK);
      goto error;
    }
    if (!PyCallable_Check(PyTuple_GET_ITEM(element, 1))) {
      PyErr_SetString(PyExc_TypeError, "hook function must be callable");
      goto error;
    }
    hooks[i].code = (EB_Hook_Code)code;
    hooks[i].function = hook_handler;
    key = PyLong_FromLong(code);
    if (key == NULL) goto error;
    status = PyDict_SetItem(callbacks, key, PyTuple_GET_ITEM(element, 1));
    Py_DECREF(key);
    if (status < 0) goto error;
  }
  hooks[size].code = EB_HOOK_NULL;
  hooks[size].function = NULL;
  status = eb_set_hooks(&hookset->hookset, hooks);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    goto error;
  }
  Py_SETREF(hookset->callbacks, callbacks);
  Py_RETURN_NONE;
error:
  Py_DECREF(callbacks);
  return NULL;
}

static char py_eb_seek_text__doc__[] = "eb_seek_text(book, (int, int)) => None Seek the file to the specified position.  Raise EBError if failed.";

static PyObject *
py_eb_seek_text(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!(ii)",
                        &BookType, &book, &pos.page, &pos.offset))
    return NULL;
  status = eb_seek_text(&book->book, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_tell_text__doc__[] = "eb_tell_text(book) => (int, int) Return the current file position.  Raise EBError if failed.";

static PyObject *
py_eb_tell_text(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_tell_text(&book->book, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return Py_BuildValue("(ii)", pos.page, pos.offset);
}

static char py_eb_read_text__doc__[] = "eb_read_text(book, appendix, hookset, container) => bytes Read a data chunk from the entry text.  In order to read all the data chunks, applications have to call this function several times until it returns a null string.  Raise EBError if failed.";

static PyObject *
read_text_chunk(PyObject *args, int heading)
{
  BookObject *book;
  AppendixObject *appendix;
  HooksetObject *hookset;
  PyObject *container, *key, *context, *error_type, *error_value, *error_tb;
  char buffer[EB_SIZE_PAGE];
  int status, active;
  ssize_t len;

  if (!PyArg_ParseTuple(args, "O!O!O!O", &BookType, &book, &AppendixType,
                        &appendix, &HooksetType, &hookset, &container))
    return NULL;
  key = PyLong_FromLong(book->book.code);
  if (key == NULL) return NULL;
  /* EB text uses a process-wide cache and a nonrecursive native mutex. */
  active = (int)PyDict_Size(CallbackContext);
  if (active != 0) {
    Py_DECREF(key);
    if (active > 0)
      PyErr_SetString(PyExc_RuntimeError, "recursive EB text reading is unsupported");
    return NULL;
  }
  context = PyTuple_Pack(3, book, appendix, hookset);
  if (context == NULL || PyDict_SetItem(CallbackContext, key, context) < 0) {
    Py_XDECREF(context);
    Py_DECREF(key);
    return NULL;
  }
  if (heading)
    status = eb_read_heading(&book->book, &appendix->appendix, &hookset->hookset,
                            container, EB_SIZE_PAGE - 1, buffer, &len);
  else
    status = eb_read_text(&book->book, &appendix->appendix, &hookset->hookset,
                         container, EB_SIZE_PAGE - 1, buffer, &len);
  /* Keep a Python callback's original exception and traceback through cleanup. */
  PyErr_Fetch(&error_type, &error_value, &error_tb);
  if (PyDict_DelItem(CallbackContext, key) < 0 && error_type != NULL)
    PyErr_Clear();
  Py_DECREF(key);
  Py_DECREF(context);
  if (error_type != NULL) PyErr_Restore(error_type, error_value, error_tb);
  if (PyErr_Occurred()) return NULL;
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyBytes_FromStringAndSize(buffer, len);
}

static PyObject *
py_eb_read_text(PyObject *self, PyObject *args)
{
  return read_text_chunk(args, 0);
}

static char py_eb_read_heading__doc__[] = "eb_read_heading(book, appendix, hookset, container) => bytes Read the next heading chunk.";

static PyObject *
py_eb_read_heading(PyObject *self, PyObject *args)
{
  return read_text_chunk(args, 1);
}

static char py_eb_read_rawtext__doc__[] = "eb_read_rawtext(book, int) => string Return the given number of raw characters read from the current file position.  Raise EBError if failed.";

static PyObject *
py_eb_read_rawtext(PyObject *self, PyObject *args)
{
  BookObject *book;
  char *buffer;
  int buffer_size, status;
  ssize_t len;
  PyObject *string;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &buffer_size))
    return NULL;
  if (buffer_size < 0) {
    PyErr_SetString(PyExc_ValueError, "raw text size must be nonnegative");
    return NULL;
  }
  if (PyDict_Size(CallbackContext) != 0) {
    PyErr_SetString(PyExc_RuntimeError, "recursive EB text reading is unsupported");
    return NULL;
  }
  if ((buffer = (char *)PyMem_Malloc((size_t)buffer_size + 1)) == NULL)
    return PyErr_NoMemory();
  status = eb_read_rawtext(&book->book, buffer_size, buffer, &len);
  if (status != EB_SUCCESS) {
    PyMem_Free(buffer);
    set_eb_error(status);
    return NULL;
  }
  string = PyString_FromStringAndSize(buffer, len);
  PyMem_Free(buffer);
  return string;
}

static char py_eb_write_text_byte1__doc__[] = ""; static PyObject * py_eb_write_text_byte1(PyObject *self, PyObject *args) {
  BookObject *book;
  int byte1;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &byte1))
    return NULL;
  return PyInt_FromLong(eb_write_text_byte1(&book->book, byte1));
}

static char py_eb_write_text_byte2__doc__[] = ""; static PyObject * py_eb_write_text_byte2(PyObject *self, PyObject *args) {
  BookObject *book;
  int byte1, byte2;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &byte1, &byte2))
    return NULL;
  return PyInt_FromLong(eb_write_text_byte2(&book->book, byte1, byte2));
}

static char py_eb_write_text_string__doc__[] = ""; static PyObject * py_eb_write_text_string(PyObject *self, PyObject *args) {
  BookObject *book;
  const char *string;

  if (!PyArg_ParseTuple(args, "O!y", &BookType, &book, &string))
    return NULL;
  return PyInt_FromLong(eb_write_text_string(&book->book, string));
}

static char py_eb_write_text__doc__[] = ""; static PyObject * py_eb_write_text(PyObject *self, PyObject *args) {
  BookObject *book;
  const char *string;
  Py_ssize_t size;

  if (!PyArg_ParseTuple(args, "O!y#", &BookType, &book, &string, &size))
    return NULL;
  if (size > INT_MAX) {
    PyErr_SetString(PyExc_OverflowError, "text buffer is too large");
    return NULL;
  }
  return PyInt_FromLong(eb_write_text(&book->book, string, (int)size));
}

static char py_eb_current_candidate__doc__[] = "eb_current_candidate(book) => string Return the current candidate of the multi search.";

static PyObject *
py_eb_current_candidate(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyString_FromString(eb_current_candidate(&book->book));
}

static char py_eb_forward_text__doc__[] = "eb_forward_text(book, appendix) => None Advance to the next text section. EB 3.x uses its native default hooks. Raise EBError if failed.";

static PyObject *
py_eb_forward_text(PyObject *self, PyObject *args)
{
  BookObject *book;
  AppendixObject *appendix;
  int status;
  if (!PyArg_ParseTuple(args, "O!O!",
                        &BookType, &book, &AppendixType, &appendix))
    return NULL;
  if (PyDict_Size(CallbackContext) != 0) {
    PyErr_SetString(PyExc_RuntimeError, "recursive EB text reading is unsupported");
    return NULL;
  }
#if EB_VERSION_MAJOR < 4
  /* EB 3.x forwards with a hookset, not an appendix. Use native defaults. */
  status = eb_forward_text(&book->book, NULL);
#else
  status = eb_forward_text(&book->book, &appendix->appendix);
#endif
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

/* unit.c */

/* [error.h] */

/* error.c */

static char py_eb_error_string__doc__[] = "eb_error_string(int) => string Return the error string of the specified error code.";

static PyObject *
py_eb_error_string(PyObject *self, PyObject *args)
{
  int error_code;

  if (!PyArg_ParseTuple(args, "i", &error_code))
    return NULL;
  return PyString_FromString(eb_error_string(error_code));
}

static char py_eb_error_message__doc__[] = "eb_error_message(int) => string Return the error message of the specified error code.";

static PyObject *
py_eb_error_message(PyObject *self, PyObject *args)
{
  int error_code;

  if (!PyArg_ParseTuple(args, "i", &error_code))
    return NULL;
  return PyString_FromString(eb_error_message(error_code));
}

/* [font.h] */

/* bitmap.c */

static char py_eb_narrow_font_xbm_size__doc__[] = "eb_narrow_font_xbm_size(int) => int Return the XBM size of the specified narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_xbm_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_narrow_font_xbm_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_narrow_font_xpm_size__doc__[] = "eb_narrow_font_xpm_size(int) => int Return the XPM size of the specified narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_xpm_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_narrow_font_xpm_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_narrow_font_gif_size__doc__[] = "eb_narrow_font_gif_size(int) => int Return the GIF size of the specified narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_gif_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_narrow_font_gif_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_wide_font_xbm_size__doc__[] = "eb_wide_font_xbm_size(int) => int Return the XBM size of the specified wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_xbm_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_wide_font_xbm_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_wide_font_xpm_size__doc__[] = "eb_wide_font_xpm_size(int) => int Return the XPM size of the specified wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_xpm_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_wide_font_xpm_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_wide_font_gif_size__doc__[] = "eb_wide_font_gif_size(int) => int Return the GIF size of the specified wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_gif_size(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_wide_font_gif_size(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_bitmap_to_xbm__doc__[] = "eb_bitmap_to_xbm(string, int, int) => string Convert the bitmap data of the specified width and height into the XBM format.";

static PyObject *
py_eb_bitmap_to_xbm(PyObject *self, PyObject *args)
{
  char *bitmap, buffer[EB_SIZE_WIDE_FONT_48_XBM];
  int width, height;
  size_t size;
  Py_ssize_t input_size;

  if (!PyArg_ParseTuple(args, "y#ii", &bitmap, &input_size, &width, &height))
    return NULL;
  /* Bound the fixed-size EB font output buffer and the input bitmap. */
  if (width <= 0 || width > EB_WIDTH_WIDE_FONT_48 || height <= 0 || height > EB_HEIGHT_FONT_48 ||
      input_size < ((width + 7) / 8) * height) {
    PyErr_SetString(PyExc_ValueError, "invalid or truncated EB font bitmap");
    return NULL;
  }
  eb_bitmap_to_xbm(bitmap, width, height, buffer, &size);
  return PyString_FromStringAndSize(buffer, size);
}

static char py_eb_bitmap_to_xpm__doc__[] = "eb_bitmap_to_xpm(string, int, int) => string Convert the bitmap data of the specified width and height into the XPM format.";

static PyObject *
py_eb_bitmap_to_xpm(PyObject *self, PyObject *args)
{
  char *bitmap, buffer[EB_SIZE_WIDE_FONT_48_XPM];
  int width, height;
  size_t size;
  Py_ssize_t input_size;

  if (!PyArg_ParseTuple(args, "y#ii", &bitmap, &input_size, &width, &height))
    return NULL;
  /* Bound the fixed-size EB font output buffer and the input bitmap. */
  if (width <= 0 || width > EB_WIDTH_WIDE_FONT_48 || height <= 0 || height > EB_HEIGHT_FONT_48 ||
      input_size < ((width + 7) / 8) * height) {
    PyErr_SetString(PyExc_ValueError, "invalid or truncated EB font bitmap");
    return NULL;
  }
  eb_bitmap_to_xpm(bitmap, width, height, buffer, &size);
  return PyString_FromStringAndSize(buffer, size);
}

static char py_eb_bitmap_to_gif__doc__[] = "eb_bitmap_to_gif(string, int, int) => string Convert the bitmap data of the specified width and height into the GIF format.";

static PyObject *
py_eb_bitmap_to_gif(PyObject *self, PyObject *args)
{
  char *bitmap, buffer[EB_SIZE_WIDE_FONT_48_GIF];
  int width, height;
  size_t size;
  Py_ssize_t input_size;

  if (!PyArg_ParseTuple(args, "y#ii", &bitmap, &input_size, &width, &height))
    return NULL;
  /* Bound the fixed-size EB font output buffer and the input bitmap. */
  if (width <= 0 || width > EB_WIDTH_WIDE_FONT_48 || height <= 0 || height > EB_HEIGHT_FONT_48 ||
      input_size < ((width + 7) / 8) * height) {
    PyErr_SetString(PyExc_ValueError, "invalid or truncated EB font bitmap");
    return NULL;
  }
  eb_bitmap_to_gif(bitmap, width, height, buffer, &size);
  return PyString_FromStringAndSize(buffer, size);
}

/* font.c */

static char py_eb_font__doc__[] = "eb_font(book) => int Return the current font's code.  Raise EBError if failed.";

static PyObject *
py_eb_font(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Font_Code font;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_font(&book->book, &font);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(font);
}

static char py_eb_set_font__doc__[] = "eb_set_font(book, int) => None Specify the current font.  Raise EBError if failed.";

static PyObject *
py_eb_set_font(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Font_Code font;
  int status;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &font))
    return NULL;
  status = eb_set_font(&book->book, font);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_unset_font__doc__[] = "eb_unset_font(book) => None Unselect the current font.";

static PyObject *
py_eb_unset_font(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  eb_unset_font(&book->book);
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_font_list__doc__[] = "eb_font_list(book) => int[] Return a tuple of font codes in the book.  Raise EBError if failed.";

static PyObject *
py_eb_font_list(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Font_Code fontlist[EB_MAX_FONTS];
  PyObject *tuple;
  int status, len, i;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_font_list(&book->book, fontlist, &len);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  if ((tuple = PyTuple_New(len)) == NULL)
    return NULL;
  for (i = 0; i < len; i++)
    PyTuple_SetItem(tuple, i, PyInt_FromLong(fontlist[i]));
  return tuple;
}

static char py_eb_have_font__doc__[] = "eb_have_font(book, int) => int Return true if the book has the font of the specified code.";

static PyObject *
py_eb_have_font(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Font_Code font;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &font))
    return NULL;
  return PyInt_FromLong(eb_have_font(&book->book, font));
}

static char py_eb_font_height__doc__[] = "eb_font_height(book) => int Return the height of the current font.  Raise EBError if failed.";

static PyObject *
py_eb_font_height(PyObject *self, PyObject *args)
{
  BookObject *book;
  int height, status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_font_height(&book->book, &height);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(height);
}

static char py_eb_font_height2__doc__[] = "eb_font_height2(int) => int Return the height of the specified font.  Raise EBError if failed.";

static PyObject *
py_eb_font_height2(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  int height, status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_font_height2(font, &height);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(height);
}

/* narwfont.c */

static char py_eb_have_narrow_font__doc__[] = "eb_narrow_have_font(book) => int Return true if the book has a narrow font.";

static PyObject *
py_eb_have_narrow_font(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_narrow_font(&book->book));
}

static char py_eb_narrow_font_width__doc__[] = "eb_narrow_font_width(book) => int Return the width of the current narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_width(PyObject *self, PyObject *args)
{
  BookObject *book;
  int width, status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_narrow_font_width(&book->book, &width);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(width);
}

static char py_eb_narrow_font_width2__doc__[] = "eb_narrow_font_width2(int) => int Return the width of the specified narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_width2(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  int width, status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_narrow_font_width2(font, &width);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(width);
}

static char py_eb_narrow_font_size__doc__[] = "eb_narrow_font_size(book) => int Return the size of the current narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_size(PyObject *self, PyObject *args)
{
  BookObject *book;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_narrow_font_size(&book->book, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_narrow_font_size2__doc__[] = "eb_narrow_font_size2(int) => int Return the size of the specified narrow font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_size2(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_narrow_font_size2(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_narrow_font_start__doc__[] = "eb_narrow_font_start(book) => int Return the first character code in the narrow font(s).  Return EBError if failed.";

static PyObject *
py_eb_narrow_font_start(PyObject *self, PyObject *args)
{
  BookObject *book;
  int status, start;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_narrow_font_start(&book->book, &start);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(start);
}

static char py_eb_narrow_font_end__doc__[] = "eb_narrow_font_end(book) => int Return the last character code in the narrow font(s).  Return EBError if failed.";

static PyObject *
py_eb_narrow_font_end(PyObject *self, PyObject *args)
{
  BookObject *book;
  int status, end;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_narrow_font_end(&book->book, &end);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(end);
}

static char py_eb_narrow_font_character_bitmap__doc__[] = "eb_narrow_font_character_bitmap(book, int) => string Return the bitmap of the specified character code in the current font.  Raise EBError if failed.";

static PyObject *
py_eb_narrow_font_character_bitmap(PyObject *self, PyObject *args)
{
  BookObject *book;
  char buffer[EB_SIZE_NARROW_FONT_48];
  int status, ch;
  size_t size;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &ch))
    return NULL;
  status = eb_narrow_font_character_bitmap(&book->book, ch, buffer);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  status = eb_narrow_font_size(&book->book, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromStringAndSize(buffer, size);
}

static char py_eb_forward_narrow_font_character__doc__[] = ""; static PyObject * py_eb_forward_narrow_font_character(PyObject *self, PyObject *args) {
  BookObject *book;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &n, &ch))
    return NULL;
  status = eb_forward_narrow_font_character(&book->book, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

static char py_eb_backward_narrow_font_character__doc__[] = ""; static PyObject * py_eb_backward_narrow_font_character(PyObject *self, PyObject *args) {
  BookObject *book;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &n, &ch))
    return NULL;
  status = eb_backward_narrow_font_character(&book->book, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

/* widefont.c */

static char py_eb_have_wide_font__doc__[] = "eb_wide_have_font(book) => int Return true if the book has a wide font.";

static PyObject *
py_eb_have_wide_font(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  return PyInt_FromLong(eb_have_wide_font(&book->book));
}

static char py_eb_wide_font_width__doc__[] = "eb_wide_font_width(book) => int Return the width of the current wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_width(PyObject *self, PyObject *args)
{
  BookObject *book;
  int width, status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_wide_font_width(&book->book, &width);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(width);
}

static char py_eb_wide_font_width2__doc__[] = "eb_wide_font_width2(int) => int Return the width of the specified wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_width2(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  int width, status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_wide_font_width2(font, &width);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(width);
}

static char py_eb_wide_font_size__doc__[] = "eb_wide_font_size(book) => int Return the size of the current wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_size(PyObject *self, PyObject *args)
{
  BookObject *book;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_wide_font_size(&book->book, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_wide_font_size2__doc__[] = "eb_wide_font_size2(int) => int Return the size of the specified wide font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_size2(PyObject *self, PyObject *args)
{
  EB_Font_Code font;
  size_t size;
  int status;

  if (!PyArg_ParseTuple(args, "i", &font))
    return NULL;
  status = eb_wide_font_size2(font, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(size);
}

static char py_eb_wide_font_start__doc__[] = "eb_wide_font_start(book) => int Return the first character code in the wide font(s).  Return EBError if failed.";

static PyObject *
py_eb_wide_font_start(PyObject *self, PyObject *args)
{
  BookObject *book;
  int status, start;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_wide_font_start(&book->book, &start);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(start);
}

static char py_eb_wide_font_end__doc__[] = "eb_wide_font_end(book) => int Return the last character code in the wide font(s).  Return EBError if failed.";

static PyObject *
py_eb_wide_font_end(PyObject *self, PyObject *args)
{
  BookObject *book;
  int status, end;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_wide_font_end(&book->book, &end);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(end);
}

static char py_eb_wide_font_character_bitmap__doc__[] = "eb_wide_font_character_bitmap(book, int) => string Return the bitmap of the specified character code in the current font.  Raise EBError if failed.";

static PyObject *
py_eb_wide_font_character_bitmap(PyObject *self, PyObject *args)
{
  BookObject *book;
  char buffer[EB_SIZE_WIDE_FONT_48];
  int status, ch;
  size_t size;

  if (!PyArg_ParseTuple(args, "O!i", &BookType, &book, &ch))
    return NULL;
  status = eb_wide_font_character_bitmap(&book->book, ch, buffer);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  status = eb_wide_font_size(&book->book, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromStringAndSize(buffer, size);
}

static char py_eb_forward_wide_font_character__doc__[] = ""; static PyObject * py_eb_forward_wide_font_character(PyObject *self, PyObject *args) {
  BookObject *book;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &n, &ch))
    return NULL;
  status = eb_forward_wide_font_character(&book->book, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

static char py_eb_backward_wide_font_character__doc__[] = ""; static PyObject * py_eb_backward_wide_font_character(PyObject *self, PyObject *args) {
  BookObject *book;
  int n, ch, status;

  if (!PyArg_ParseTuple(args, "O!ii", &BookType, &book, &n, &ch))
    return NULL;
  status = eb_backward_wide_font_character(&book->book, n, &ch);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyInt_FromLong(ch);
}

/* [binary.h] */

/* binary.c */

static char py_eb_set_binary_mono_graphic__doc__[] = "eb_set_binary_mono_graphic(book, (int, int), int, int) => None Set a monochrome graphic as the current binary data.  Raise EBError if failed.";

static PyObject *
py_eb_set_binary_mono_graphic(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int width, height, status;

  if (!PyArg_ParseTuple(args, "O!(ii)ii", &BookType, &book,
                        &pos.page, &pos.offset, &width, &height))
    return NULL;
  status = eb_set_binary_mono_graphic(&book->book, &pos, width, height);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_set_binary_gray_graphic__doc__[] = "eb_set_binary_gray_graphic(book, (int, int), int, int) => None Set a gray scale graphic as the current binary data.  Raise EBError if failed.";

static PyObject *
py_eb_set_binary_gray_graphic(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int width, height, status;

  if (!PyArg_ParseTuple(args, "O!(ii)ii", &BookType, &book,
                        &pos.page, &pos.offset, &width, &height))
    return NULL;
  status = eb_set_binary_gray_graphic(&book->book, &pos, width, height);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_set_binary_color_graphic__doc__[] = "eb_set_binary_color_graphic(book, (int, int)) => None Set a color graphic as the current binary data.  Raise EBError if failed.";

static PyObject *
py_eb_set_binary_color_graphic(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position pos;
  int status;

  if (!PyArg_ParseTuple(args, "O!(ii)",
                        &BookType, &book, &pos.page, &pos.offset))
    return NULL;
  status = eb_set_binary_color_graphic(&book->book, &pos);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_set_binary_wave__doc__[] = "eb_set_binary_wave(book, (int, int), (int, int)) => None Set a WAVE chunk as the current binary data.  Raise EBError if failed.";

static PyObject *
py_eb_set_binary_wave(PyObject *self, PyObject *args)
{
  BookObject *book;
  EB_Position start, end;
  int status;

  if (!PyArg_ParseTuple(args, "O!(ii)(ii)", &BookType, &book,
                        &start.page, &start.offset, &end.page, &end.offset))
    return NULL;
  status = eb_set_binary_wave(&book->book, &start, &end);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static char py_eb_read_binary__doc__[] = "eb_read_binary(book) => string Read a data chunk of the current binary data.  In order to read all the data chunks, applications have to call this function several times until it returns a null string.  Raise EBError if failed.";

static PyObject *
py_eb_read_binary(PyObject *self, PyObject *args)
{
  BookObject *book;
  char buffer[EB_SIZE_PAGE];
  int status;
  ssize_t size;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  status = eb_read_binary(&book->book, EB_SIZE_PAGE, buffer, &size);
  if (status != EB_SUCCESS) {
    set_eb_error(status);
    return NULL;
  }
  return PyString_FromStringAndSize(buffer, size);
}

static char py_eb_unset_binary__doc__[] = "eb_unset_binary(book) => None Unselect the current binary.";

static PyObject *
py_eb_unset_binary(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
    return NULL;
  eb_unset_binary(&book->book);
  Py_INCREF(Py_None);
  return Py_None;
}


static char py_eb_is_text_stopped__doc__[] = "eb_is_text_stopped(book) => True/False if reached a stop code while reading text (see EBlib documentation).";

static PyObject *
py_eb_is_text_stopped(PyObject *self, PyObject *args)
{
  BookObject *book;

  if (!PyArg_ParseTuple(args, "O!", &BookType, &book))
      return NULL;

#if EB_VERSION_MAJOR < 4
  return PyInt_FromLong(book->book.text_context.text_end_flag);
#else
  return PyInt_FromLong(eb_is_text_stopped(&book->book));
#endif
}

/* List of methods defined in the eb module */

#define meth(name, func, doc) {name, (PyCFunction)func, METH_VARARGS, doc}

static struct PyMethodDef eb_module_methods[] = {
  /* [eb.h] */
  meth("eb_initialize_library",
       py_eb_initialize_library,
       py_eb_initialize_library__doc__),
  meth("eb_finalize_library",
       py_eb_finalize_library,
       py_eb_finalize_library__doc__),
  /* eb.c */
  meth("EB_Book", /* eb_initialize_book(book) */
       py_eb_Book,
       py_eb_Book__doc__),
  meth("eb_bind",
       py_eb_bind,
       py_eb_bind__doc__),
  meth("eb_suspend",
       py_eb_suspend,
       py_eb_suspend__doc__),
  meth("eb_is_bound",
       py_eb_is_bound,
       py_eb_is_bound__doc__),
  meth("eb_path",
       py_eb_path,
       py_eb_path__doc__),
  meth("eb_disc_type",
       py_eb_disc_type,
       py_eb_disc_type__doc__),
  meth("eb_character_code",
       py_eb_character_code,
       py_eb_character_code__doc__),
  /* copyright.c */
  meth("eb_have_copyright",
       py_eb_have_copyright,
       py_eb_have_copyright__doc__),
  meth("eb_copyright",
       py_eb_copyright,
       py_eb_copyright__doc__),
  /* menu.c */
  meth("eb_have_menu",
       py_eb_have_menu,
       py_eb_have_menu__doc__),
  meth("eb_menu",
       py_eb_menu,
       py_eb_menu__doc__),
  /* exactword.c */
  meth("eb_have_exactword_search",
       py_eb_have_exactword_search,
       py_eb_have_exactword_search__doc__),
  meth("eb_search_exactword",
       py_eb_search_exactword,
       py_eb_search_exactword__doc__),
  /* word.c */
  meth("eb_have_word_search",
       py_eb_have_word_search,
       py_eb_have_word_search__doc__),
  meth("eb_search_word",
       py_eb_search_word,
       py_eb_search_word__doc__),
  /* endword.c */
  meth("eb_have_endword_search",
       py_eb_have_endword_search,
       py_eb_have_endword_search__doc__),
  meth("eb_search_endword",
       py_eb_search_endword,
       py_eb_search_endword__doc__),
  /* keyword.c */
  meth("eb_have_keyword_search",
       py_eb_have_keyword_search,
       py_eb_have_keyword_search__doc__),
  meth("eb_search_keyword",
       py_eb_search_keyword,
       py_eb_search_keyword__doc__),
  /* multi.c */
  meth("eb_have_multi_search",
       py_eb_have_multi_search,
       py_eb_have_multi_search__doc__),
  meth("eb_multi_search_list",
       py_eb_multi_search_list,
       py_eb_multi_search_list__doc__),
  meth("eb_multi_entry_list",
       py_eb_multi_entry_list,
       py_eb_multi_entry_list__doc__),
  meth("eb_multi_entry_label",
       py_eb_multi_entry_label,
       py_eb_multi_entry_label__doc__),
  meth("eb_multi_entry_have_candidates",
       py_eb_multi_entry_have_candidates,
       py_eb_multi_entry_have_candidates__doc__),
  meth("eb_multi_entry_candidates",
       py_eb_multi_entry_candidates,
       py_eb_multi_entry_candidates__doc__),
  /* search.c */
  meth("eb_hit_list",
       py_eb_hit_list,
       py_eb_hit_list__doc__),
  /* subbook.c */
  meth("eb_load_all_subbooks",
       py_eb_load_all_subbooks,
       py_eb_load_all_subbooks__doc__),
  meth("eb_subbook_list",
       py_eb_subbook_list,
       py_eb_subbook_list__doc__),
  meth("eb_subbook",
       py_eb_subbook,
       py_eb_subbook__doc__),
  meth("eb_subbook_title",
       py_eb_subbook_title,
       py_eb_subbook_title__doc__),
  meth("eb_subbook_title2",
       py_eb_subbook_title2,
       py_eb_subbook_title2__doc__),
  meth("eb_subbook_directory",
       py_eb_subbook_directory,
       py_eb_subbook_directory__doc__),
  meth("eb_subbook_directory2",
       py_eb_subbook_directory2,
       py_eb_subbook_directory2__doc__),
  meth("eb_set_subbook",
       py_eb_set_subbook,
       py_eb_set_subbook__doc__),
  meth("eb_unset_subbook",
       py_eb_unset_subbook,
       py_eb_unset_subbook__doc__),
  /* lock.c */
  meth("eb_pthread_enabled",
       py_eb_pthread_enabled,
       py_eb_pthread_enabled__doc__),
  /* log.c */
  /* [appendix.h] */
  /* appendix.c */
  meth("EB_Appendix", /* eb_initialize_appendix(appendix) */
       py_eb_Appendix,
       py_eb_Appendix__doc__),
  meth("eb_suspend_appendix",
       py_eb_suspend_appendix,
       py_eb_suspend_appendix__doc__),
  meth("eb_bind_appendix",
       py_eb_bind_appendix,
       py_eb_bind_appendix__doc__),
  meth("eb_is_appendix_bound",
       py_eb_is_appendix_bound,
       py_eb_is_appendix_bound__doc__),
  meth("eb_appendix_path",
       py_eb_appendix_path,
       py_eb_appendix_path__doc__),
  /* appsub.c */
  meth("eb_load_all_appendix_subbooks",
       py_eb_load_all_appendix_subbooks,
       py_eb_load_all_appendix_subbooks__doc__),
  meth("eb_appendix_subbook_list",
       py_eb_appendix_subbook_list,
       py_eb_appendix_subbook_list__doc__),
  meth("eb_appendix_subbook",
       py_eb_appendix_subbook,
       py_eb_appendix_subbook__doc__),
  meth("eb_appendix_subbook_directory",
       py_eb_appendix_subbook_directory,
       py_eb_appendix_subbook_directory__doc__),
  meth("eb_appendix_subbook_directory2",
       py_eb_appendix_subbook_directory2,
       py_eb_appendix_subbook_directory2__doc__),
  meth("eb_set_appendix_subbook",
       py_eb_set_appendix_subbook,
       py_eb_set_appendix_subbook__doc__),
  meth("eb_unset_appendix_subbook",
       py_eb_unset_appendix_subbook,
       py_eb_unset_appendix_subbook__doc__),
  /* stopcode.c */
  meth("eb_have_stop_code",
       py_eb_have_stop_code,
       py_eb_have_stop_code__doc__),
  meth("eb_stop_code",
       py_eb_stop_code,
       py_eb_stop_code__doc__),
  /* narwalt.c */
  meth("eb_have_narrow_alt",
       py_eb_have_narrow_alt,
       py_eb_have_narrow_alt__doc__),
  meth("eb_narrow_alt_start",
       py_eb_narrow_alt_start,
       py_eb_narrow_alt_start__doc__),
  meth("eb_narrow_alt_end",
       py_eb_narrow_alt_end,
       py_eb_narrow_alt_end__doc__),
  meth("eb_narrow_alt_character_text",
       py_eb_narrow_alt_character_text,
       py_eb_narrow_alt_character_text__doc__),
  meth("eb_forward_narrow_alt_character",
       py_eb_forward_narrow_alt_character,
       py_eb_forward_narrow_alt_character__doc__),
  meth("eb_backward_narrow_alt_character",
       py_eb_backward_narrow_alt_character,
       py_eb_backward_narrow_alt_character__doc__),
  /* widealt.c */
  meth("eb_have_wide_alt",
       py_eb_have_wide_alt,
       py_eb_have_wide_alt__doc__),
  meth("eb_wide_alt_start",
       py_eb_wide_alt_start,
       py_eb_wide_alt_start__doc__),
  meth("eb_wide_alt_end",
       py_eb_wide_alt_end,
       py_eb_wide_alt_end__doc__),
  meth("eb_wide_alt_character_text",
       py_eb_wide_alt_character_text,
       py_eb_wide_alt_character_text__doc__),
  meth("eb_forward_wide_alt_character",
       py_eb_forward_wide_alt_character,
       py_eb_forward_wide_alt_character__doc__),
  meth("eb_backward_wide_alt_character",
       py_eb_backward_wide_alt_character,
       py_eb_backward_wide_alt_character__doc__),
  /* [text.h] */
  /* hook.c */
  meth("eb_hook_euc_to_ascii",
       py_eb_hook_euc_to_ascii,
       py_eb_hook_euc_to_ascii__doc__),
  meth("eb_hook_stop_code",
       py_eb_hook_stop_code,
       py_eb_hook_stop_code__doc__),
  meth("eb_hook_narrow_character_text",
       py_eb_hook_narrow_character_text,
       py_eb_hook_narrow_character_text__doc__),
  meth("eb_hook_wide_character_text",
       py_eb_hook_wide_character_text,
       py_eb_hook_wide_character_text__doc__),
  meth("eb_hook_newline",
       py_eb_hook_newline,
       py_eb_hook_newline__doc__),
  /*meth("eb_hook_empty",
       py_eb_hook_empty,
       py_eb_hook_empty__doc__),*/
  /* text.c */
  meth("EB_Hookset", /* eb_initialize_hookset(hookset) */
       py_eb_Hookset,
       py_eb_Hookset__doc__),
  meth("eb_set_hook",
       py_eb_set_hook,
       py_eb_set_hook__doc__),
  meth("eb_set_hooks",
       py_eb_set_hooks,
       py_eb_set_hooks__doc__),
  meth("eb_seek_text",
       py_eb_seek_text,
       py_eb_seek_text__doc__),
  meth("eb_tell_text",
       py_eb_tell_text,
       py_eb_tell_text__doc__),
  meth("eb_read_text",
       py_eb_read_text,
       py_eb_read_text__doc__),
  meth("eb_read_heading",
       py_eb_read_heading,
       py_eb_read_heading__doc__),
  meth("eb_read_rawtext",
       py_eb_read_rawtext,
       py_eb_read_rawtext__doc__),
  meth("eb_write_text_byte1",
       py_eb_write_text_byte1,
       py_eb_write_text_byte1__doc__),
  meth("eb_write_text_byte2",
       py_eb_write_text_byte2,
       py_eb_write_text_byte2__doc__),
  meth("eb_write_text_string",
       py_eb_write_text_string,
       py_eb_write_text_string__doc__),
  meth("eb_write_text",
       py_eb_write_text,
       py_eb_write_text__doc__),
  meth("eb_current_candidate",
       py_eb_current_candidate,
       py_eb_current_candidate__doc__),
  meth("eb_forward_text",
       py_eb_forward_text,
       py_eb_forward_text__doc__),
  /* uint.c */
  /* [error.h] */
  /* error.c */
  meth("eb_error_string",
       py_eb_error_string,
       py_eb_error_string__doc__),
  meth("eb_error_message",
       py_eb_error_message,
       py_eb_error_message__doc__),
  /* [language.h] */
  /* language.c */
  /* message.c */
  /* [font.h] */
  /* bitmap.c */
  meth("eb_narrow_font_xbm_size",
       py_eb_narrow_font_xbm_size,
       py_eb_narrow_font_xbm_size__doc__),
  meth("eb_narrow_font_xpm_size",
       py_eb_narrow_font_xpm_size,
       py_eb_narrow_font_xpm_size__doc__),
  meth("eb_narrow_font_gif_size",
       py_eb_narrow_font_gif_size,
       py_eb_narrow_font_gif_size__doc__),
  meth("eb_wide_font_xbm_size",
       py_eb_wide_font_xbm_size,
       py_eb_wide_font_xbm_size__doc__),
  meth("eb_wide_font_xpm_size",
       py_eb_wide_font_xpm_size,
       py_eb_wide_font_xpm_size__doc__),
  meth("eb_wide_font_gif_size",
       py_eb_wide_font_gif_size,
       py_eb_wide_font_gif_size__doc__),
  meth("eb_bitmap_to_xbm",
       py_eb_bitmap_to_xbm,
       py_eb_bitmap_to_xbm__doc__),
  meth("eb_bitmap_to_xpm",
       py_eb_bitmap_to_xpm,
       py_eb_bitmap_to_xpm__doc__),
  meth("eb_bitmap_to_gif",
       py_eb_bitmap_to_gif,
       py_eb_bitmap_to_gif__doc__),
  /* font.c */
  meth("eb_font",
       py_eb_font,
       py_eb_font__doc__),
  meth("eb_set_font",
       py_eb_set_font,
       py_eb_set_font__doc__),
  meth("eb_unset_font",
       py_eb_unset_font,
       py_eb_unset_font__doc__),
  meth("eb_font_list",
       py_eb_font_list,
       py_eb_font_list__doc__),
  meth("eb_have_font",
       py_eb_have_font,
       py_eb_have_font__doc__),
  meth("eb_font_height",
       py_eb_font_height,
       py_eb_font_height__doc__),
  meth("eb_font_height2",
       py_eb_font_height2,
       py_eb_font_height2__doc__),
  /* narwfont.c */
  meth("eb_have_narrow_font",
       py_eb_have_narrow_font,
       py_eb_have_narrow_font__doc__),
  meth("eb_narrow_font_width",
       py_eb_narrow_font_width,
       py_eb_narrow_font_width__doc__),
  meth("eb_narrow_font_width2",
       py_eb_narrow_font_width2,
       py_eb_narrow_font_width2__doc__),
  meth("eb_narrow_font_size",
       py_eb_narrow_font_size,
       py_eb_narrow_font_size__doc__),
  meth("eb_narrow_font_size2",
       py_eb_narrow_font_size2,
       py_eb_narrow_font_size2__doc__),
  meth("eb_narrow_font_start",
       py_eb_narrow_font_start,
       py_eb_narrow_font_start__doc__),
  meth("eb_narrow_font_end",
       py_eb_narrow_font_end,
       py_eb_narrow_font_end__doc__),
  meth("eb_narrow_font_character_bitmap",
       py_eb_narrow_font_character_bitmap,
       py_eb_narrow_font_character_bitmap__doc__),
  meth("eb_forward_narrow_font_character",
       py_eb_forward_narrow_font_character,
       py_eb_forward_narrow_font_character__doc__),
  meth("eb_backward_narrow_font_character",
       py_eb_backward_narrow_font_character,
       py_eb_backward_narrow_font_character__doc__),
  /* widefont.c */
  meth("eb_have_wide_font",
       py_eb_have_wide_font,
       py_eb_have_wide_font__doc__),
  meth("eb_wide_font_width",
       py_eb_wide_font_width,
       py_eb_wide_font_width__doc__),
  meth("eb_wide_font_width2",
       py_eb_wide_font_width2,
       py_eb_wide_font_width2__doc__),
  meth("eb_wide_font_size",
       py_eb_wide_font_size,
       py_eb_wide_font_size__doc__),
  meth("eb_wide_font_size2",
       py_eb_wide_font_size2,
       py_eb_wide_font_size2__doc__),
  meth("eb_wide_font_start",
       py_eb_wide_font_start,
       py_eb_wide_font_start__doc__),
  meth("eb_wide_font_end",
       py_eb_wide_font_end,
       py_eb_wide_font_end__doc__),
  meth("eb_wide_font_character_bitmap",
       py_eb_wide_font_character_bitmap,
       py_eb_wide_font_character_bitmap__doc__),
  meth("eb_forward_wide_font_character",
       py_eb_forward_wide_font_character,
       py_eb_forward_wide_font_character__doc__),
  meth("eb_backward_wide_font_character",
       py_eb_backward_wide_font_character,
       py_eb_backward_wide_font_character__doc__),
  /* [binary.h] */
  /* binary.c */
  meth("eb_set_binary_mono_graphic",
       py_eb_set_binary_mono_graphic,
       py_eb_set_binary_mono_graphic__doc__),
  meth("eb_set_binary_gray_graphic",
       py_eb_set_binary_gray_graphic,
       py_eb_set_binary_gray_graphic__doc__),
  meth("eb_set_binary_color_graphic",
       py_eb_set_binary_color_graphic,
       py_eb_set_binary_color_graphic__doc__),
  meth("eb_set_binary_wave",
       py_eb_set_binary_wave,
       py_eb_set_binary_wave__doc__),
  meth("eb_read_binary",
       py_eb_read_binary,
       py_eb_read_binary__doc__),
  meth("eb_unset_binary",
       py_eb_unset_binary,
       py_eb_unset_binary__doc__),
  /* filename.c */

  /* my extensions */
  meth("eb_is_text_stopped",
       py_eb_is_text_stopped,
       py_eb_is_text_stopped__doc__),

  {NULL, (PyCFunction)NULL, 0, NULL} /* sentinel */
};

/* Python 3 module initializer. */

static char eb_module_documentation[] = "";

static struct PyModuleDef eb_module = {
  PyModuleDef_HEAD_INIT, "eb", eb_module_documentation, -1, eb_module_methods
};

PyMODINIT_FUNC
PyInit_eb(void)
{
  PyObject *m;
  if (PyType_Ready(&BookType) < 0 || PyType_Ready(&AppendixType) < 0 ||
      PyType_Ready(&HooksetType) < 0) return NULL;
  m = PyModule_Create(&eb_module);
  if (m == NULL) return NULL;
  if (PyModule_AddStringConstant(m, "version", version) < 0) goto error;
  EBError = PyErr_NewException("eb.EBError", NULL, NULL);
  if (EBError == NULL || PyModule_AddObjectRef(m, "EBError", EBError) < 0) goto error;
  CallbackContext = PyDict_New();
  if (CallbackContext == NULL || PyModule_AddObjectRef(m, "_callback_context", CallbackContext) < 0) goto error;

#define constI(dict, name, value) \
        if (PyModule_AddIntConstant(m, name, value) < 0) goto error;

  /* [defs.h] */
  /* Library version */
  constI(d, "EB_VERSION_MAJOR",              EB_VERSION_MAJOR);
  constI(d, "EB_VERSION_MINOR",              EB_VERSION_MINOR);
  /* Disc code */
  constI(d, "EB_DISC_EB",                    EB_DISC_EB);
  constI(d, "EB_DISC_EPWING",                EB_DISC_EPWING);
  constI(d, "EB_DISC_INVALID",               EB_DISC_INVALID);
  /* Character codes */
  constI(d, "EB_CHARCODE_ISO8859_1",         EB_CHARCODE_ISO8859_1);
  constI(d, "EB_CHARCODE_JISX0208",          EB_CHARCODE_JISX0208);
  constI(d, "EB_CHARCODE_JISX0208_GB2312",   EB_CHARCODE_JISX0208_GB2312);
  constI(d, "EB_CHARCODE_INVALID",           EB_CHARCODE_INVALID);
  /* Special book ID for cache */
  constI(d, "EB_BOOK_NONE",                  EB_BOOK_NONE);
  /* Special error states */  
  constI(d, "EB_SUBBOOK_INVALID",            EB_SUBBOOK_INVALID);
  constI(d, "EB_MULTI_INVALID",              EB_MULTI_INVALID);
  /*constI(d, "EB_MULTI_ENTRY_INVALID",        EB_MULTI_ENTRY_INVALID);*/
  /* Sizes and limitations */
  constI(d, "EB_SIZE_PAGE",                  EB_SIZE_PAGE);
  /*constI(d, "EB_SIZE_EB_CATALOG",            EB_SIZE_EB_CATALOG);*/
  /*constI(d, "EB_SIZE_EPWING_CATALOG",        EB_SIZE_EPWING_CATALOG);*/
  /*constI(d, "EB_SIZE_EBZIP_HEADER",          EB_SIZE_EBZIP_HEADER);*/
  /*constI(d, "EB_SIZE_EBZIP_MARGIN",          EB_SIZE_EBZIP_MARGIN);*/
  constI(d, "EB_MAX_WORD_LENGTH",            EB_MAX_WORD_LENGTH);
  constI(d, "EB_MAX_EB_TITLE_LENGTH",        EB_MAX_EB_TITLE_LENGTH);
  constI(d, "EB_MAX_EPWING_TITLE_LENGTH",    EB_MAX_EPWING_TITLE_LENGTH);
  constI(d, "EB_MAX_TITLE_LENGTH",           EB_MAX_TITLE_LENGTH);
  constI(d, "EB_MAX_DIRECTORY_NAME_LENGTH",  EB_MAX_DIRECTORY_NAME_LENGTH);
  constI(d, "EB_MAX_FILE_NAME_LENGTH",       EB_MAX_FILE_NAME_LENGTH);
  constI(d, "EB_MAX_MULTI_LABEL_LENGTH",     EB_MAX_MULTI_LABEL_LENGTH);
  constI(d, "EB_MAX_ALTERNATION_TEXT_LENGTH",EB_MAX_ALTERNATION_TEXT_LENGTH);
  constI(d, "EB_MAX_FONTS",                  EB_MAX_FONTS);
  constI(d, "EB_MAX_SUBBOOKS",               EB_MAX_SUBBOOKS);
  constI(d, "EB_MAX_KEYWORDS",               EB_MAX_KEYWORDS);
  constI(d, "EB_MAX_MULTI_SEARCHES",         EB_MAX_MULTI_SEARCHES);
  constI(d, "EB_MAX_MULTI_ENTRIES",          EB_MAX_MULTI_ENTRIES);
  constI(d, "EB_MAX_ALTERNATION_CACHE",      EB_MAX_ALTERNATION_CACHE);
  /* Hook */
  constI(d, "EB_NUMBER_OF_HOOKS",            EB_NUMBER_OF_HOOKS);
  constI(d, "EB_NUMBER_OF_SEARCH_CONTEXTS",  EB_NUMBER_OF_SEARCH_CONTEXTS);
  /* [text.h] */
  /* Hook codes */
  constI(d, "EB_HOOK_NULL",                  EB_HOOK_NULL);
  constI(d, "EB_HOOK_INITIALIZE",            EB_HOOK_INITIALIZE);
  constI(d, "EB_HOOK_BEGIN_NARROW",          EB_HOOK_BEGIN_NARROW);
  constI(d, "EB_HOOK_END_NARROW",            EB_HOOK_END_NARROW);
  constI(d, "EB_HOOK_BEGIN_SUBSCRIPT",       EB_HOOK_BEGIN_SUBSCRIPT);
  constI(d, "EB_HOOK_END_SUBSCRIPT",         EB_HOOK_END_SUBSCRIPT);
  constI(d, "EB_HOOK_SET_INDENT",            EB_HOOK_SET_INDENT);
  constI(d, "EB_HOOK_NEWLINE",               EB_HOOK_NEWLINE);
  constI(d, "EB_HOOK_BEGIN_SUPERSCRIPT",     EB_HOOK_BEGIN_SUPERSCRIPT);
  constI(d, "EB_HOOK_END_SUPERSCRIPT",       EB_HOOK_END_SUPERSCRIPT);
  constI(d, "EB_HOOK_BEGIN_NO_NEWLINE",      EB_HOOK_BEGIN_NO_NEWLINE);
  constI(d, "EB_HOOK_END_NO_NEWLINE",        EB_HOOK_END_NO_NEWLINE);
  constI(d, "EB_HOOK_BEGIN_EMPHASIS",        EB_HOOK_BEGIN_EMPHASIS);
  constI(d, "EB_HOOK_END_EMPHASIS",          EB_HOOK_END_EMPHASIS);
  constI(d, "EB_HOOK_BEGIN_CANDIDATE",       EB_HOOK_BEGIN_CANDIDATE);
  constI(d, "EB_HOOK_END_CANDIDATE_GROUP",   EB_HOOK_END_CANDIDATE_GROUP);
  constI(d, "EB_HOOK_END_CANDIDATE_LEAF",    EB_HOOK_END_CANDIDATE_LEAF);
  constI(d, "EB_HOOK_BEGIN_REFERENCE",       EB_HOOK_BEGIN_REFERENCE);
  constI(d, "EB_HOOK_END_REFERENCE",         EB_HOOK_END_REFERENCE);
  constI(d, "EB_HOOK_BEGIN_KEYWORD",         EB_HOOK_BEGIN_KEYWORD);
  constI(d, "EB_HOOK_END_KEYWORD",           EB_HOOK_END_KEYWORD);
  constI(d, "EB_HOOK_NARROW_FONT",           EB_HOOK_NARROW_FONT);
  constI(d, "EB_HOOK_WIDE_FONT",             EB_HOOK_WIDE_FONT);
  constI(d, "EB_HOOK_ISO8859_1",             EB_HOOK_ISO8859_1);
  constI(d, "EB_HOOK_NARROW_JISX0208",       EB_HOOK_NARROW_JISX0208);
  constI(d, "EB_HOOK_WIDE_JISX0208",         EB_HOOK_WIDE_JISX0208);
  constI(d, "EB_HOOK_GB2312",                EB_HOOK_GB2312);
  constI(d, "EB_HOOK_STOP_CODE",             EB_HOOK_STOP_CODE);
  constI(d, "EB_HOOK_BEGIN_MONO_GRAPHIC",    EB_HOOK_BEGIN_MONO_GRAPHIC);
  constI(d, "EB_HOOK_END_MONO_GRAPHIC",      EB_HOOK_END_MONO_GRAPHIC); 
  constI(d, "EB_HOOK_BEGIN_GRAY_GRAPHIC",    EB_HOOK_BEGIN_GRAY_GRAPHIC);
  constI(d, "EB_HOOK_END_GRAY_GRAPHIC",      EB_HOOK_END_GRAY_GRAPHIC);
  constI(d, "EB_HOOK_BEGIN_COLOR_BMP",       EB_HOOK_BEGIN_COLOR_BMP);
  constI(d, "EB_HOOK_BEGIN_COLOR_JPEG",      EB_HOOK_BEGIN_COLOR_JPEG);
  constI(d, "EB_HOOK_END_COLOR_GRAPHIC",     EB_HOOK_END_COLOR_GRAPHIC);
  constI(d, "EB_HOOK_BEGIN_IN_COLOR_BMP",    EB_HOOK_BEGIN_IN_COLOR_BMP);
  constI(d, "EB_HOOK_BEGIN_IN_COLOR_JPEG",   EB_HOOK_BEGIN_IN_COLOR_JPEG);
  constI(d, "EB_HOOK_END_IN_COLOR_GRAPHIC",  EB_HOOK_END_IN_COLOR_GRAPHIC);
  constI(d, "EB_HOOK_BEGIN_WAVE",            EB_HOOK_BEGIN_WAVE);
  constI(d, "EB_HOOK_END_WAVE",              EB_HOOK_END_WAVE);
  constI(d, "EB_HOOK_BEGIN_MPEG",            EB_HOOK_BEGIN_MPEG);
  constI(d, "EB_HOOK_END_MPEG",              EB_HOOK_END_MPEG);
#ifdef EB_HOOK_BEGIN_DECORATION
  constI(d, "EB_HOOK_BEGIN_DECORATION",      EB_HOOK_BEGIN_DECORATION);
  constI(d, "EB_HOOK_END_DECORATION",        EB_HOOK_END_DECORATION);
#endif
  /* [error.h] */
  /* Error codes */
  constI(d, "EB_SUCCESS",                    EB_SUCCESS);
  constI(d, "EB_ERR_MEMORY_EXHAUSTED",       EB_ERR_MEMORY_EXHAUSTED);
  constI(d, "EB_ERR_EMPTY_FILE_NAME",        EB_ERR_EMPTY_FILE_NAME);
  constI(d, "EB_ERR_TOO_LONG_FILE_NAME",     EB_ERR_TOO_LONG_FILE_NAME);
  constI(d, "EB_ERR_BAD_FILE_NAME",          EB_ERR_BAD_FILE_NAME);
  constI(d, "EB_ERR_BAD_DIR_NAME",           EB_ERR_BAD_DIR_NAME);
  constI(d, "EB_ERR_TOO_LONG_WORD",          EB_ERR_TOO_LONG_WORD);
  constI(d, "EB_ERR_BAD_WORD",               EB_ERR_BAD_WORD);
  constI(d, "EB_ERR_EMPTY_WORD",             EB_ERR_EMPTY_WORD);
  constI(d, "EB_ERR_FAIL_GETCWD",            EB_ERR_FAIL_GETCWD);
  constI(d, "EB_ERR_FAIL_OPEN_CAT",          EB_ERR_FAIL_OPEN_CAT);
  constI(d, "EB_ERR_FAIL_OPEN_CATAPP",       EB_ERR_FAIL_OPEN_CATAPP);
  constI(d, "EB_ERR_FAIL_OPEN_TEXT",         EB_ERR_FAIL_OPEN_TEXT);
  constI(d, "EB_ERR_FAIL_OPEN_FONT",         EB_ERR_FAIL_OPEN_FONT);
  constI(d, "EB_ERR_FAIL_OPEN_APP",          EB_ERR_FAIL_OPEN_APP);
  constI(d, "EB_ERR_FAIL_OPEN_BINARY",       EB_ERR_FAIL_OPEN_BINARY);
  constI(d, "EB_ERR_FAIL_READ_CAT",          EB_ERR_FAIL_READ_CAT);
  constI(d, "EB_ERR_FAIL_READ_CATAPP",       EB_ERR_FAIL_READ_CATAPP);
  constI(d, "EB_ERR_FAIL_READ_TEXT",         EB_ERR_FAIL_READ_TEXT);
  constI(d, "EB_ERR_FAIL_READ_FONT",         EB_ERR_FAIL_READ_FONT);
  constI(d, "EB_ERR_FAIL_READ_APP",          EB_ERR_FAIL_READ_APP);
  constI(d, "EB_ERR_FAIL_READ_BINARY",       EB_ERR_FAIL_READ_BINARY);
  constI(d, "EB_ERR_FAIL_SEEK_CAT",          EB_ERR_FAIL_SEEK_CAT);
  constI(d, "EB_ERR_FAIL_SEEK_CATAPP",       EB_ERR_FAIL_SEEK_CATAPP);
  constI(d, "EB_ERR_FAIL_SEEK_TEXT",         EB_ERR_FAIL_SEEK_TEXT);
  constI(d, "EB_ERR_FAIL_SEEK_FONT",         EB_ERR_FAIL_SEEK_FONT);
  constI(d, "EB_ERR_FAIL_SEEK_APP",          EB_ERR_FAIL_SEEK_APP);
  constI(d, "EB_ERR_FAIL_SEEK_BINARY",       EB_ERR_FAIL_SEEK_BINARY);
  constI(d, "EB_ERR_UNEXP_CAT",              EB_ERR_UNEXP_CAT);
  constI(d, "EB_ERR_UNEXP_CATAPP",           EB_ERR_UNEXP_CATAPP);
  constI(d, "EB_ERR_UNEXP_TEXT",             EB_ERR_UNEXP_TEXT);
  constI(d, "EB_ERR_UNEXP_FONT",             EB_ERR_UNEXP_FONT);
  constI(d, "EB_ERR_UNEXP_APP",              EB_ERR_UNEXP_APP);
  constI(d, "EB_ERR_UNEXP_BINARY",           EB_ERR_UNEXP_BINARY);
  constI(d, "EB_ERR_UNBOUND_BOOK",           EB_ERR_UNBOUND_BOOK);
  constI(d, "EB_ERR_UNBOUND_APP",            EB_ERR_UNBOUND_APP);
  constI(d, "EB_ERR_NO_SUB",                 EB_ERR_NO_SUB);
  constI(d, "EB_ERR_NO_APPSUB",              EB_ERR_NO_APPSUB);
  constI(d, "EB_ERR_NO_FONT",                EB_ERR_NO_FONT);
  constI(d, "EB_ERR_NO_TEXT",                EB_ERR_NO_TEXT);
  constI(d, "EB_ERR_NO_CUR_SUB",             EB_ERR_NO_CUR_SUB);
  constI(d, "EB_ERR_NO_CUR_APPSUB",          EB_ERR_NO_CUR_APPSUB);
  constI(d, "EB_ERR_NO_CUR_FONT",            EB_ERR_NO_CUR_FONT);
  constI(d, "EB_ERR_NO_CUR_BINARY",          EB_ERR_NO_CUR_BINARY);
  constI(d, "EB_ERR_NO_SUCH_SUB",            EB_ERR_NO_SUCH_SUB);
  constI(d, "EB_ERR_NO_SUCH_APPSUB",         EB_ERR_NO_SUCH_APPSUB);
  constI(d, "EB_ERR_NO_SUCH_FONT",           EB_ERR_NO_SUCH_FONT);
  constI(d, "EB_ERR_NO_SUCH_CHAR_BMP",       EB_ERR_NO_SUCH_CHAR_BMP);
  constI(d, "EB_ERR_NO_SUCH_CHAR_TEXT",      EB_ERR_NO_SUCH_CHAR_TEXT);
  constI(d, "EB_ERR_NO_SUCH_SEARCH",         EB_ERR_NO_SUCH_SEARCH);
  constI(d, "EB_ERR_NO_SUCH_HOOK",           EB_ERR_NO_SUCH_HOOK);
  constI(d, "EB_ERR_NO_SUCH_BINARY",         EB_ERR_NO_SUCH_BINARY);
  /*constI(d, "EB_ERR_STOP_CODE",              EB_ERR_STOP_CODE);*/
  constI(d, "EB_ERR_DIFF_CONTENT",           EB_ERR_DIFF_CONTENT);
  constI(d, "EB_ERR_NO_PREV_SEARCH",         EB_ERR_NO_PREV_SEARCH);
  constI(d, "EB_ERR_NO_SUCH_MULTI_ID",       EB_ERR_NO_SUCH_MULTI_ID);
  constI(d, "EB_ERR_NO_SUCH_ENTRY_ID",       EB_ERR_NO_SUCH_ENTRY_ID);
  constI(d, "EB_ERR_TOO_MANY_WORDS",         EB_ERR_TOO_MANY_WORDS);
  constI(d, "EB_ERR_NO_WORD",                EB_ERR_NO_WORD);
  constI(d, "EB_ERR_NO_CANDIDATES",          EB_ERR_NO_CANDIDATES);
  constI(d, "EB_NUMBER_OF_ERRORS",           EB_NUMBER_OF_ERRORS);
  constI(d, "EB_MAX_ERROR_MESSAGE_LENGTH",   EB_MAX_ERROR_MESSAGE_LENGTH);
#ifdef EB_ERR_END_OF_CONTENT
  constI(d, "EB_ERR_END_OF_CONTENT",         EB_ERR_END_OF_CONTENT);
#endif
  /* [font.h] */   
  /* Font types */
  constI(d, "EB_FONT_16",                    EB_FONT_16);
  constI(d, "EB_FONT_24",                    EB_FONT_24);
  constI(d, "EB_FONT_30",                    EB_FONT_30);
  constI(d, "EB_FONT_48",                    EB_FONT_48);
  constI(d, "EB_FONT_INVALID",               EB_FONT_INVALID);
  /* Font sizes */
  constI(d, "EB_SIZE_NARROW_FONT_16",        EB_SIZE_NARROW_FONT_16);
  constI(d, "EB_SIZE_WIDE_FONT_16",          EB_SIZE_WIDE_FONT_16);
  constI(d, "EB_SIZE_NARROW_FONT_24",        EB_SIZE_NARROW_FONT_24);
  constI(d, "EB_SIZE_WIDE_FONT_24",          EB_SIZE_WIDE_FONT_24);
  constI(d, "EB_SIZE_NARROW_FONT_30",        EB_SIZE_NARROW_FONT_30);
  constI(d, "EB_SIZE_WIDE_FONT_30",          EB_SIZE_WIDE_FONT_30);
  constI(d, "EB_SIZE_NARROW_FONT_48",        EB_SIZE_NARROW_FONT_48);
  constI(d, "EB_SIZE_WIDE_FONT_48",          EB_SIZE_WIDE_FONT_48);
  /* Font widths */
  constI(d, "EB_WIDTH_NARROW_FONT_16",       EB_WIDTH_NARROW_FONT_16);
  constI(d, "EB_WIDTH_WIDE_FONT_16",         EB_WIDTH_WIDE_FONT_16);
  constI(d, "EB_WIDTH_NARROW_FONT_24",       EB_WIDTH_NARROW_FONT_24);
  constI(d, "EB_WIDTH_WIDE_FONT_24",         EB_WIDTH_WIDE_FONT_24);
  constI(d, "EB_WIDTH_NARROW_FONT_30",       EB_WIDTH_NARROW_FONT_30);
  constI(d, "EB_WIDTH_WIDE_FONT_30",         EB_WIDTH_WIDE_FONT_30);
  constI(d, "EB_WIDTH_NARROW_FONT_48",       EB_WIDTH_NARROW_FONT_48);
  constI(d, "EB_WIDTH_WIDE_FONT_48",         EB_WIDTH_WIDE_FONT_48);
  /* Font heights */
  constI(d, "EB_HEIGHT_FONT_16",             EB_HEIGHT_FONT_16);
  constI(d, "EB_HEIGHT_FONT_24",             EB_HEIGHT_FONT_24);
  constI(d, "EB_HEIGHT_FONT_30",             EB_HEIGHT_FONT_30);
  constI(d, "EB_HEIGHT_FONT_48",             EB_HEIGHT_FONT_48);
  /* Bitmap image sizes */
  constI(d, "EB_SIZE_NARROW_FONT_16_XBM",    EB_SIZE_NARROW_FONT_16_XBM);
  constI(d, "EB_SIZE_WIDE_FONT_16_XBM",      EB_SIZE_WIDE_FONT_16_XBM);
  constI(d, "EB_SIZE_NARROW_FONT_16_XPM",    EB_SIZE_NARROW_FONT_16_XPM);
  constI(d, "EB_SIZE_WIDE_FONT_16_XPM",      EB_SIZE_WIDE_FONT_16_XPM);
  constI(d, "EB_SIZE_NARROW_FONT_16_GIF",    EB_SIZE_NARROW_FONT_16_GIF);
  constI(d, "EB_SIZE_WIDE_FONT_16_GIF",      EB_SIZE_WIDE_FONT_16_GIF);
  constI(d, "EB_SIZE_NARROW_FONT_24_XBM",    EB_SIZE_NARROW_FONT_24_XBM);
  constI(d, "EB_SIZE_WIDE_FONT_24_XBM",      EB_SIZE_WIDE_FONT_24_XBM);
  constI(d, "EB_SIZE_NARROW_FONT_24_XPM",    EB_SIZE_NARROW_FONT_24_XPM);
  constI(d, "EB_SIZE_WIDE_FONT_24_XPM",      EB_SIZE_WIDE_FONT_24_XPM);
  constI(d, "EB_SIZE_NARROW_FONT_24_GIF",    EB_SIZE_NARROW_FONT_24_GIF);
  constI(d, "EB_SIZE_WIDE_FONT_24_GIF",      EB_SIZE_WIDE_FONT_24_GIF);
  constI(d, "EB_SIZE_NARROW_FONT_30_XBM",    EB_SIZE_NARROW_FONT_30_XBM);
  constI(d, "EB_SIZE_WIDE_FONT_30_XBM",      EB_SIZE_WIDE_FONT_30_XBM);
  constI(d, "EB_SIZE_NARROW_FONT_30_XPM",    EB_SIZE_NARROW_FONT_30_XPM);
  constI(d, "EB_SIZE_WIDE_FONT_30_XPM",      EB_SIZE_WIDE_FONT_30_XPM);
  constI(d, "EB_SIZE_NARROW_FONT_30_GIF",    EB_SIZE_NARROW_FONT_30_GIF);
  constI(d, "EB_SIZE_WIDE_FONT_30_GIF",      EB_SIZE_WIDE_FONT_30_GIF);
  constI(d, "EB_SIZE_NARROW_FONT_48_XBM",    EB_SIZE_NARROW_FONT_48_XBM);
  constI(d, "EB_SIZE_WIDE_FONT_48_XBM",      EB_SIZE_WIDE_FONT_48_XBM);
  constI(d, "EB_SIZE_NARROW_FONT_48_XPM",    EB_SIZE_NARROW_FONT_48_XPM);
  constI(d, "EB_SIZE_WIDE_FONT_48_XPM",      EB_SIZE_WIDE_FONT_48_XPM);
  constI(d, "EB_SIZE_NARROW_FONT_48_GIF",    EB_SIZE_NARROW_FONT_48_GIF);
  constI(d, "EB_SIZE_WIDE_FONT_48_GIF",      EB_SIZE_WIDE_FONT_48_GIF);
  constI(d, "EB_SIZE_FONT_IMAGE",            EB_SIZE_FONT_IMAGE);

  if (PyErr_Occurred()) goto error;
  return m;
error:
  Py_CLEAR(EBError);
  Py_CLEAR(CallbackContext);
  Py_DECREF(m);
  return NULL;
}
