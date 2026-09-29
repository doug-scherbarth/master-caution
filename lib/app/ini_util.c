// lib/app/ini_util.c
#include "ini_util.h"
#include <string.h>

void ini_str_lower(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s += 32;
}

int ini_next_line(const char **pp, char *buf, int bufsz) {
    if (!**pp) return 0;
    int n = 0;
    while (**pp && **pp != '\n' && n < bufsz - 1)
        buf[n++] = *(*pp)++;
    if (**pp == '\n') (*pp)++;
    while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        n--;
    buf[n] = '\0';
    return n;
}

void ini_strip_comment(char *val) {
    for (char *p = val; *p; p++) {
        if (*p == ';' || *p == '#') { *p = '\0'; break; }
    }
    int n = (int)strlen(val);
    while (n > 0 && (val[n-1] == ' ' || val[n-1] == '\t')) val[--n] = '\0';
}
