# Wi Modules and Foreign Libraries

This directory is for Wi modules and foreign libraries.

The `import "name"` statement looks for:

- `name.wi` or `name/init.wi` (Wi modules)
- `name.so` or `name.dll` (Linux or Windows foreign libraries)

**In this exact directory.** For example, `import "name";` will look for `name.wi`, `name/init.wi`, or `name.so` here.
