#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <category/core/format_err.h>

static char const *final_path_component(char const *path)
{
    char const *last = strrchr(path, '/');
    return last == nullptr ? path : last + 1;
}

extern int monad_vformat_err(
    char *err_buf, size_t err_buf_size, struct monad_source_location const *src,
    int err, char const *format, va_list ap)
{
    size_t len;
    int rc = 0;

    if (src != nullptr) {
        rc = snprintf(
            err_buf,
            err_buf_size,
            "%s@%s:%u",
            src->function_name,
            final_path_component(src->file_name),
            src->line);
    }
    len = rc > 0 ? (size_t)rc : 0;
    if (len < err_buf_size - 2) {
        err_buf[len++] = ':';
        err_buf[len++] = ' ';
        rc = vsnprintf(err_buf + len, err_buf_size - len, format, ap);
        if (rc >= 0) {
            len += (size_t)rc;
        }
    }
    if (err != 0 && len < err_buf_size) {
        (void)snprintf(
            err_buf + len, err_buf_size - len, ": %s (%d)", strerror(err), err);
    }
    return err;
}
