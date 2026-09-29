// lib/app/ini_util.h
// Shared helpers for INI-style config file parsers (alarm_cfg, light_cfg).

#ifndef INI_UTIL_H
#define INI_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

// Lowercase s in-place (ASCII letters only).
void ini_str_lower(char *s);

// Read one line from *pp into buf (without the newline), advance *pp.
// Strips trailing CR/space/tab. Returns line length; 0 on empty or exhausted.
int ini_next_line(const char **pp, char *buf, int bufsz);

// Strip an inline ';' or '#' comment (and trailing whitespace) from val.
// Operates in-place on a NUL-terminated string.
void ini_strip_comment(char *val);

#ifdef __cplusplus
}
#endif

#endif // INI_UTIL_H
