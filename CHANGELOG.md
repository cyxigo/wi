## [Unreleased]

## Added

- WI_RUN_OOM (API)
- wi_state_pcall/wi_pcall_fn (API)

## Changed

- wi_push_module now takes an explicit module path (foreign libraries must be rebuilt)
- wi_check_version now takes the minor version too, WI_CHECK_VERSION is updated (foreign libraries must be rebuilt)
- wi_load, wi_def_std, wi_def_stm now return wi_run_result (foreign libraries and embedders must be updated)

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
