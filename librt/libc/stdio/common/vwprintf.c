#include <stdio.h>
#include <stdarg.h>

int vwprintf(
    _In_ __CONST wchar_t *format, 
    _In_ va_list valist)
{
    return vfwprintf(stdout, format, valist);
}
