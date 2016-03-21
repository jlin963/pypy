/*** rpython/translator/rsandbox/src/part.h ***/

#ifndef _RSANDBOX_H_
#define _RSANDBOX_H_

#ifndef RPY_SANDBOX_EXPORTED
/* Common definitions when including this file from an external C project */

#include <stdlib.h>
#include <sys/utsname.h>

#define RPY_SANDBOX_EXPORTED  extern

typedef long Signed;
typedef unsigned long Unsigned;

#endif


/* ***********************************************************

   WARNING: Python is not meant to be a safe language.  For example,
   think about making a custom code object with a random string and
   trying to interpret that.  A sandboxed PyPy contains extra safety
   checks that can detect such invalid operations before they cause
   problems.  When such a case is detected, THE WHOLE PROCESS IS
   ABORTED right now.  In the future, there should be a setjmp/longjmp
   alternative to this, but the details need a bit of care (e.g. it
   would still create memory leaks).

   For now, you have to accept that the process can be aborted if
   given malicious code.  Also, running several Python sources from
   different sources in the same process is not recommended---there is
   only one global state: malicious code can easily mangle the state
   of the Python interpreter, influencing subsequent runs.  Unless you
   are fine with both issues, you MUST run Python from subprocesses,
   not from your main program.

   Multi-threading issues: DO NOT USE FROM SEVERAL THREADS AT THE SAME
   TIME!  You need a lock.  If you use subprocesses, they will likely
   be single-threaded anyway.  (This issue might be fixed in the
   future.  Note that the sandboxed Python itself doesn't have the
   possibility of starting threads.)
*/


/* This configures the maximum heap size allowed to the Python
   interpreter.  It only accounts for GC-tracked objects, so actual
   memory usage can be larger.  (It should hopefully never be more
   than about twice the value, but for the paranoid, you should not
   use this.  You should do setrlimit() to bound the total RAM usage
   of the subprocess.  Similarly, you have no direct way to bound the
   amount of time spent inside Python, but it is easy to set up an
   alarm signal with alarm().)
*/
void rsandbox_set_heap_size(size_t maximum);


/* Entry point: rsandbox_open() loads the given source code inside a
   new Python module.  The source code should define the interesting
   Python functions, but cannot actually compute stuff yet: you cannot
   pass here arguments or return values.  rsandbox_open() returns a
   module pointer if execution succeeds, or NULL if it gets an
   exception.  The pointer can be used in rsandbox_call().  It can
   optionally be freed with rsandbox_close().

   You can use this function with source code that is assembled from
   untrusted sources, but it is recommended to pass a constant string
   here.  You can pass extra arguments with 'rpython_push_*()',
   declared below; they are visible as 'args[0]', 'args[1]', and so
   on.  This allows you to do things like this:

   rsandbox_module_t *compile_expression(const char *expression)
   {
       rsandbox_push_string(expression);   // 'expression' is untrusted
       return rsandbox_open(
           "code = compile(args[0], '<untrusted>', 'eval')\n"
           "def evaluate(n):\n"
           "    return eval(code, {'n': n})\n")
   }

   long safely_evaluate(rsandbox_module_t *mod, long n_value)
   {
       rsandbox_push_long(n_value);
       rsandbox_call(mod, "evaluate");   // ignore exceptions
       return rsandbox_result_long();    // result; if any problem, will be 0
   }
*/
typedef struct rsandbox_module_s rsandbox_module_t;
RPY_SANDBOX_EXPORTED rsandbox_module_t *rsandbox_open(const char *src);
RPY_SANDBOX_EXPORTED void rsandbox_close(rsandbox_module_t *mod);

/* To call one of the Python functions defined in the module, first
   push the arguments one by one with rsandbox_push_*(), then call
   rsandbox_call().  If an exception occur, -1 is returned.

   rsandbox_push_rw_buffer() is a way to pass read-write data.  From
   the Python side, this will pass a read-write buffer object.  After
   rsandbox_call() returns, the buffer becomes invalid.
   (rsandbox_push_rw_buffer() is not very useful for rsandbox_open():
   the buffer becomes invalid as soon as rsandbox_open() returns.)
*/
RPY_SANDBOX_EXPORTED void rsandbox_push_long(long);
RPY_SANDBOX_EXPORTED void rsandbox_push_double(double);
RPY_SANDBOX_EXPORTED void rsandbox_push_string(const char *);
RPY_SANDBOX_EXPORTED void rsandbox_push_string_and_size(const char *, size_t);
RPY_SANDBOX_EXPORTED void rsandbox_push_none(void);
RPY_SANDBOX_EXPORTED void rsandbox_push_rw_buffer(char *, size_t);

RPY_SANDBOX_EXPORTED int rsandbox_call(rsandbox_module_t *mod,
                                       const char *func_name);

/* Returns the result of the previous rsandbox_call() if the Python
   function returned an 'int' object.  Otherwise, returns 0.  (You
   MUST NOT assume anything about the 'long': careful with malicious
   code returning results like sys.maxint or -sys.maxint-1.) */
RPY_SANDBOX_EXPORTED long rsandbox_result_long(void);

/* Returns the result of the previous rsandbox_call() if the Python
   function returned an 'int' or 'float' object.  Otherwise, 0.0.
   (You MUST NOT assume anything about the 'double': careful with
   malicious code returning results like inf, nan, or 1e-323.) */
RPY_SANDBOX_EXPORTED double rsandbox_result_double(void);

/* Returns the length of the string returned in the previous
   rsandbox_call().  If it was not a string, returns 0. */
RPY_SANDBOX_EXPORTED size_t rsandbox_result_string_length(void);

/* Returns the data in the string.  This function always writes an
   additional '\0'.  If the string is longer than 'bufsize-1', it is
   truncated to 'bufsize-1' characters.

   For small human-readable strings you can call
   rsandbox_result_string() with some fixed maximum size.  You get a
   regular null-terminated 'char *' string.  (If it contains embedded
   '\0', it will appear truncated; if the Python function did not
   return a string at all, it will be completely empty; but anyway
   you MUST be ready to handle any malformed string at all.)

   For strings of larger sizes or strings that can meaningfully
   contain embedded '\0', you should allocate a 'buf' of size
   'rsandbox_result_string_length() + 1'.

   To repeat: Be careful when reading strings from Python!  They can
   contain any character, so be sure to escape them correctly (or
   reject them outright) if, for example, you are passing them
   further.  Malicious code can return any string.  Your code must be
   ready for anything.  Err on the side of caution.
*/
RPY_SANDBOX_EXPORTED void rsandbox_result_string(char *buf, size_t bufsize);

/* When an exception occurred in rsandbox_open() or rsandbox_call(),
   return more information as a string.  Same rules as
   rsandbox_result_string().  (Careful, you MUST NOT assume that the
   string is well-formed: malicious code can make it contain anything.
   If you are copying it to a web page, for example, then a good idea
   is to replace any character not in a whitelist with '?'.)
*/
RPY_SANDBOX_EXPORTED void rsandbox_last_exception(char *buf, size_t bufsize,
                                                  int include_traceback);


/************************************************************/


/* The list of 'rsandbox_fnptr_*' function pointers is automatically
   generated.  Most of these function pointers are initialized to
   point to a function that aborts the sandboxed execution.  The
   sandboxed program cannot, by default, use any of them.  A few
   exceptions are provided, where the default implementation returns a
   safe default (for example rsandbox_fnptr_getenv()).
*/
