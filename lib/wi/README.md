# Wi Modules and Foreign Libraries

This directory is for Wi modules and foreign libraries.

The `import "name"` statement looks for:

- `name.wi` or `name/init.wi` (Wi modules)
- `name.so` or `name.dll` (Linux or Windows foreign libraries)

**In this exact directory.** For example, `import "math";` will look for `math.wi`, `math/init.wi`, or `math.so` here.
