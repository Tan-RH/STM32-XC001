#include "xc001_utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *XC001_Trim(char *s)
{
  char *end;

  if (s == 0)
  {
    return s;
  }
  while (*s != '\0' && isspace((unsigned char)*s))
  {
    s++;
  }
  end = s + strlen(s);
  while (end > s && isspace((unsigned char)*(end - 1)))
  {
    end--;
  }
  *end = '\0';
  return s;
}

int XC001_StrCaseCmp(const char *a, const char *b)
{
  while (*a != '\0' && *b != '\0')
  {
    int ca = toupper((unsigned char)*a++);
    int cb = toupper((unsigned char)*b++);
    if (ca != cb)
    {
      return ca - cb;
    }
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int XC001_StrNCaseCmp(const char *a, const char *b, size_t n)
{
  while (n-- > 0U)
  {
    int ca = toupper((unsigned char)*a++);
    int cb = toupper((unsigned char)*b++);
    if (ca != cb || ca == '\0' || cb == '\0')
    {
      return ca - cb;
    }
  }
  return 0;
}

const char *XC001_SkipSpace(const char *s)
{
  while (s != 0 && *s != '\0' && isspace((unsigned char)*s))
  {
    s++;
  }
  return s;
}

uint8_t XC001_ParseU16(const char *s, uint16_t *value)
{
  char *end;
  unsigned long v;

  if (s == 0 || value == 0)
  {
    return 0U;
  }
  v = strtoul(s, &end, 0);
  end = (char *)XC001_SkipSpace(end);
  if (*end != '\0' || v > 65535UL)
  {
    return 0U;
  }
  *value = (uint16_t)v;
  return 1U;
}

uint8_t XC001_ParseU32(const char *s, uint32_t max_value, uint32_t *value)
{
  char *end;
  unsigned long v;

  if (s == 0 || value == 0)
  {
    return 0U;
  }
  s = XC001_SkipSpace(s);
  if (*s == '\0' || *s == '-')
  {
    return 0U;
  }
  v = strtoul(s, &end, 0);
  end = (char *)XC001_SkipSpace(end);
  if (*end != '\0' || v > (unsigned long)max_value)
  {
    return 0U;
  }
  *value = (uint32_t)v;
  return 1U;
}

uint8_t XC001_ParseHexBytes(const char *s, uint8_t *out, uint8_t max_len, uint8_t *out_len)
{
  uint8_t len = 0U;

  if (s == 0 || out == 0 || out_len == 0)
  {
    return 0U;
  }
  while (*s != '\0')
  {
    char b[3] = {0, 0, 0};
    char *end;
    unsigned long v;

    s = XC001_SkipSpace(s);
    if (*s == '\0')
    {
      break;
    }
    if (*s == ',')
    {
      s++;
      continue;
    }
    if (len >= max_len || !isxdigit((unsigned char)s[0]))
    {
      return 0U;
    }
    b[0] = s[0];
    if (isxdigit((unsigned char)s[1]))
    {
      b[1] = s[1];
      s += 2;
    }
    else
    {
      s += 1;
    }
    v = strtoul(b, &end, 16);
    if (*end != '\0' || v > 255UL)
    {
      return 0U;
    }
    out[len++] = (uint8_t)v;
  }
  *out_len = len;
  return 1U;
}

void XC001_FormatHexBytes(const uint8_t *data, uint8_t len, char *out, size_t out_size)
{
  size_t used = 0U;

  if (out == 0 || out_size == 0U)
  {
    return;
  }
  out[0] = '\0';
  for (uint8_t i = 0; i < len; i++)
  {
    int n = snprintf(out + used, out_size - used, "%s%02X", (i == 0U) ? "" : " ", data[i]);
    if (n < 0 || (size_t)n >= out_size - used)
    {
      out[out_size - 1U] = '\0';
      return;
    }
    used += (size_t)n;
  }
}

uint8_t XC001_UrlDecode(const char *src, char *dst, size_t dst_size)
{
  size_t used = 0U;

  if (src == 0 || dst == 0 || dst_size == 0U)
  {
    return 0U;
  }
  while (*src != '\0' && used + 1U < dst_size)
  {
    if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2]))
    {
      char h[3] = {src[1], src[2], 0};
      dst[used++] = (char)strtoul(h, 0, 16);
      src += 3;
    }
    else if (*src == '+')
    {
      dst[used++] = ' ';
      src++;
    }
    else
    {
      dst[used++] = *src++;
    }
  }
  dst[used] = '\0';
  return (*src == '\0') ? 1U : 0U;
}
