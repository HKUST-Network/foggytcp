// Linux linker-only routing of upstream diagnostics. No transport logic changes.
#include <cstdarg>
#include <cstdio>
extern "C" int __wrap_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}
extern "C" int __wrap_fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream == stdout ? stderr : stream, format, args);
    va_end(args);
    return result;
}
