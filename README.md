# CJSON - JSON Parser in C
A simple JSON parser made in C, compiled and tested on Linux (Ubuntu 20.04, GCC 10.2.0) and Windows (GCC 10.2.0).

This is a sample project, used as an education tool to become more proficient in C

## Building
The library only consists of two files: `cjson.h` and its counterpart `cjson.c`. To compile it you can just pass this to your preferred compiler. You can also use the script files `build-linux.sh` or `build-win32.sh` (this will build the example executable with `main.c`).

## TODO
* Documentation and examples
* Utility functions (such as convenient lookup functions, i.e. `key>depth1>depth2>depth3>[4]`);.
* Optimise memory usage (currently the internal `cjson_state*` hogs up a lot of memory during parse);
* Optimise general speed of parsing (replace function calls?).
* Figure out if everything I've done is good or bad C :^)
* Add proper multi-threading support when memory-logging is enabled.
* Fix some memory leaks that happen when parsing fails (in specific `cjson.c!cjson_parse_impl`)

