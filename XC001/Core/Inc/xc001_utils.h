#ifndef XC001_UTILS_H
#define XC001_UTILS_H

#include <stdint.h>
#include <stddef.h>

char *XC001_Trim(char *s);
int XC001_StrCaseCmp(const char *a, const char *b);
int XC001_StrNCaseCmp(const char *a, const char *b, size_t n);
const char *XC001_SkipSpace(const char *s);
uint8_t XC001_ParseU16(const char *s, uint16_t *value);
uint8_t XC001_ParseHexBytes(const char *s, uint8_t *out, uint8_t max_len, uint8_t *out_len);
void XC001_FormatHexBytes(const uint8_t *data, uint8_t len, char *out, size_t out_size);
uint8_t XC001_UrlDecode(const char *src, char *dst, size_t dst_size);

#endif
