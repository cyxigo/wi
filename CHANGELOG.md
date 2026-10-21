## [Unreleased]

## Added

- WI_FFI_STACK_MIN (API)
- WI_NORETURN (API)
- WI_RUN_OOM (API)
- wi_state_reserve_stack (API)
- wi_state_pcall/wi_pcall_fn (API)

## Changed

- wi_push_module now takes an explicit module path (foreign libraries must be rebuilt)
- wi_check_version now takes the minor version and a name too, WI_CHECK_VERSION is updated (foreign libraries must be rebuilt)
- wi_load, wi_def_std, wi_def_stm now return wi_run_result (foreign libraries and embedders must be updated)
- Paths passed to wi_state_run, wi_load and wi_push_module are now copied, they no longer need to outlive the state
- "too many constants" compile error calling functions "prototypes" (technically correct but confusing and can be mistaken for prototype as in prototype-based OOP)
- Callback arity mismatches (e.g. array->each with a function of the wrong arity) now report the arity mismatch more clearly
- Foreign functions can now push up to WI_FFI_STACK_MIN values without triggering a stack overflow error
- wi_state_error and wi_state_abort are now marked noreturn in wi.h
- Calling a method via -> on a value that has no methods now reports "cannot use operator '->' on a value of type X" like the other operators

## Fixed

- wi_load reporting a stale EOF error from a previous compile
- Old generation accumulating under heavy allocation pressure
- Lexer errors in the first token not being reported
- "expected X" errors after a string pointing at its closing quote
- if without else emitting an unnecessary jump opcode
- io::close, io::flush, io::write, io::read, io::writebytes, io::readbytes, and io::seek not reporting what actually caused the failure
- Windows mangling non-ASCII paths, environment variables and command-line arguments
- Out of memory being indistinguishable from a regular runtime error (now returns WI_RUN_OOM)
- Failed compilations leaving their globals defined (e.g. a multi-line REPL input reporting "already defined")
- Scripts starting with a UTF-8 BOM failing to compile
- Local variable limit being 254 instead of the 255 reported in the error
- Out of memory leaking internal buffers (call frames, GC stacks, compiler locals and upvalues, function expression compilers)
- Stack overflow leaking the directory handle of os::listdir
- WI_DEF_FOREIGN_ALL, WI_OBJECT_SET_FOREIGN_ALL and WI_MODULE_EXPORT_FOREIGN_ALL breaking when their arguments used i or entry (because they are all macros)
- Stack overflow errors leaking buffers and file handles in foreign functions
- std::input and the REPL not showing the prompt before reading on some platforms
- Ctrl+C ending a script with exit status 0 (now 130)
- A backslash at the end of a line in a string literal reporting a corrupted error on the wrong line (now "unfinished string")
- std::try failing with a stack overflow when called with 255 arguments
- Error carets being misaligned on lines indented with tabs
