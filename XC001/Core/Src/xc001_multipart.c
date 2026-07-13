#include "xc001_multipart.h"
#include <ctype.h>
#include <string.h>

enum
{
  MP_STATE_START_BOUNDARY = 0,
  MP_STATE_HEADERS,
  MP_STATE_DATA,
  MP_STATE_BOUNDARY_SUFFIX,
  MP_STATE_BOUNDARY_SUFFIX_FINAL,
  MP_STATE_BOUNDARY_SUFFIX_NEXT,
  MP_STATE_DONE
};

static int ascii_case_cmp_n(const char *a, const char *b, size_t n)
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

static uint8_t starts_with_ci(const char *s, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  return (ascii_case_cmp_n(s, prefix, prefix_len) == 0) ? 1U : 0U;
}

static const char *skip_space_local(const char *s)
{
  while (*s == ' ' || *s == '\t')
  {
    s++;
  }
  return s;
}

static uint8_t param_value_copy(const char *line, const char *name,
                                char *out, size_t out_size)
{
  size_t name_len = strlen(name);
  const char *p = strchr(line, ':');

  if (out == 0 || out_size == 0U)
  {
    return 0U;
  }
  out[0] = '\0';
  if (p == 0)
  {
    return 0U;
  }
  p++;
  while (*p != '\0')
  {
    p = skip_space_local(p);
    if (*p == ';')
    {
      p++;
      continue;
    }
    if (ascii_case_cmp_n(p, name, name_len) == 0 && p[name_len] == '=')
    {
      const char *v = p + name_len + 1U;
      const char *e;
      size_t n;

      if (*v == '"')
      {
        v++;
        e = strchr(v, '"');
        if (e == 0)
        {
          return 0U;
        }
      }
      else
      {
        e = v;
        while (*e != '\0' && *e != ';' && *e != ' ' && *e != '\t')
        {
          e++;
        }
      }
      n = (size_t)(e - v);
      if (n >= out_size)
      {
        n = out_size - 1U;
      }
      memcpy(out, v, n);
      out[n] = '\0';
      return 1U;
    }
    p = strchr(p, ';');
    if (p == 0)
    {
      break;
    }
  }
  return 0U;
}

static void strip_line_end(char *line)
{
  size_t n = strlen(line);

  while (n > 0U && (line[n - 1U] == '\n' || line[n - 1U] == '\r'))
  {
    line[--n] = '\0';
  }
}

static uint8_t append_line_byte(XC001_MultipartParser *parser, uint8_t c,
                                uint8_t *ready)
{
  *ready = 0U;
  if (parser->line_len >= (sizeof(parser->line) - 1U))
  {
    parser->failed = 1U;
    return 0U;
  }
  parser->line[parser->line_len++] = (char)c;
  parser->line[parser->line_len] = '\0';
  if (c == '\n')
  {
    strip_line_end(parser->line);
    parser->line_len = 0U;
    *ready = 1U;
  }
  return 1U;
}

static void reset_part(XC001_MultipartParser *parser)
{
  parser->part_is_file = 0U;
  parser->file_active = 0U;
  parser->pending_len = 0U;
  parser->filename[0] = '\0';
}

static uint8_t parse_header_line(XC001_MultipartParser *parser)
{
  if (starts_with_ci(parser->line, "Content-Disposition:"))
  {
    char name[24];

    if (param_value_copy(parser->line, "name", name, sizeof(name)) &&
        (strcmp(name, "file") == 0 || strcmp(name, "fw") == 0))
    {
      parser->part_is_file = 1U;
      (void)param_value_copy(parser->line, "filename", parser->filename,
                             sizeof(parser->filename));
    }
  }
  return 1U;
}

static uint8_t start_part_data(XC001_MultipartParser *parser)
{
  parser->state = MP_STATE_DATA;
  parser->pending_len = 0U;
  if (parser->part_is_file != 0U)
  {
    if (parser->file_seen != 0U)
    {
      parser->failed = 1U;
      return 0U;
    }
    parser->file_seen = 1U;
    parser->file_active = 1U;
    if (parser->cb.on_file_begin != 0 &&
        parser->cb.on_file_begin(parser->cb.ctx, parser->filename) == 0U)
    {
      parser->failed = 1U;
      return 0U;
    }
  }
  return 1U;
}

static uint8_t emit_pending_byte(XC001_MultipartParser *parser)
{
  uint8_t byte = parser->pending[0];

  memmove(parser->pending, parser->pending + 1U, parser->pending_len - 1U);
  parser->pending_len--;
  if (parser->file_active != 0U)
  {
    if (parser->cb.on_file_data != 0 &&
        parser->cb.on_file_data(parser->cb.ctx, &byte, 1U) == 0U)
    {
      parser->failed = 1U;
      return 0U;
    }
    parser->file_bytes++;
  }
  return 1U;
}

static uint8_t append_part_byte(XC001_MultipartParser *parser, uint8_t c)
{
  if (parser->pending_len >= sizeof(parser->pending))
  {
    parser->failed = 1U;
    return 0U;
  }
  parser->pending[parser->pending_len++] = c;
  while (parser->pending_len > parser->delimiter_len)
  {
    if (!emit_pending_byte(parser))
    {
      return 0U;
    }
  }
  if (parser->pending_len == parser->delimiter_len &&
      memcmp(parser->pending, parser->delimiter, parser->delimiter_len) == 0)
  {
    if (parser->file_active != 0U && parser->cb.on_file_end != 0)
    {
      parser->cb.on_file_end(parser->cb.ctx);
    }
    parser->file_active = 0U;
    parser->pending_len = 0U;
    parser->state = MP_STATE_BOUNDARY_SUFFIX;
    return 1U;
  }
  return 1U;
}

uint8_t XC001_Multipart_Init(XC001_MultipartParser *parser,
                             const char *boundary,
                             const XC001_MultipartCallbacks *callbacks)
{
  size_t boundary_len;

  if (parser == 0 || boundary == 0 || callbacks == 0)
  {
    return 0U;
  }
  boundary_len = strlen(boundary);
  if (boundary_len == 0U || boundary_len > XC001_MULTIPART_BOUNDARY_MAX)
  {
    return 0U;
  }
  memset(parser, 0, sizeof(*parser));
  parser->boundary_line[0] = '-';
  parser->boundary_line[1] = '-';
  memcpy(parser->boundary_line + 2U, boundary, boundary_len);
  parser->boundary_line[boundary_len + 2U] = '\0';
  parser->boundary_line_len = boundary_len + 2U;
  parser->delimiter[0] = '\r';
  parser->delimiter[1] = '\n';
  memcpy(parser->delimiter + 2U, parser->boundary_line,
         parser->boundary_line_len + 1U);
  parser->delimiter_len = parser->boundary_line_len + 2U;
  parser->cb = *callbacks;
  parser->state = MP_STATE_START_BOUNDARY;
  return 1U;
}

uint8_t XC001_Multipart_Execute(XC001_MultipartParser *parser,
                                const uint8_t *data,
                                uint32_t length)
{
  if (parser == 0 || (data == 0 && length != 0U) || parser->failed != 0U)
  {
    return 0U;
  }

  for (uint32_t i = 0U; i < length; i++)
  {
    uint8_t ready;
    uint8_t c = data[i];

    switch (parser->state)
    {
      case MP_STATE_START_BOUNDARY:
        if (!append_line_byte(parser, c, &ready))
        {
          return 0U;
        }
        if (ready != 0U)
        {
          if (strcmp(parser->line, parser->boundary_line) != 0)
          {
            parser->failed = 1U;
            return 0U;
          }
          reset_part(parser);
          parser->state = MP_STATE_HEADERS;
        }
        break;

      case MP_STATE_HEADERS:
        if (!append_line_byte(parser, c, &ready))
        {
          return 0U;
        }
        if (ready != 0U)
        {
          if (parser->line[0] == '\0')
          {
            if (!start_part_data(parser))
            {
              return 0U;
            }
          }
          else if (!parse_header_line(parser))
          {
            return 0U;
          }
        }
        break;

      case MP_STATE_DATA:
        if (!append_part_byte(parser, c))
        {
          return 0U;
        }
        break;

      case MP_STATE_BOUNDARY_SUFFIX:
        if (c == '-')
        {
          parser->state = MP_STATE_BOUNDARY_SUFFIX_FINAL;
        }
        else if (c == '\r')
        {
          parser->state = MP_STATE_BOUNDARY_SUFFIX_NEXT;
        }
        else
        {
          parser->failed = 1U;
          return 0U;
        }
        break;

      case MP_STATE_BOUNDARY_SUFFIX_FINAL:
        if (c != '-')
        {
          parser->failed = 1U;
          return 0U;
        }
        parser->body_done = 1U;
        parser->state = MP_STATE_DONE;
        break;

      case MP_STATE_BOUNDARY_SUFFIX_NEXT:
        if (c != '\n')
        {
          parser->failed = 1U;
          return 0U;
        }
        reset_part(parser);
        parser->state = MP_STATE_HEADERS;
        break;

      case MP_STATE_DONE:
        break;

      default:
        parser->failed = 1U;
        return 0U;
    }
  }
  return 1U;
}

uint8_t XC001_Multipart_IsDone(const XC001_MultipartParser *parser)
{
  return (parser != 0 && parser->body_done != 0U && parser->failed == 0U) ? 1U : 0U;
}

uint8_t XC001_Multipart_SawFile(const XC001_MultipartParser *parser)
{
  return (parser != 0 && parser->file_seen != 0U && parser->failed == 0U) ? 1U : 0U;
}

uint32_t XC001_Multipart_FileBytes(const XC001_MultipartParser *parser)
{
  return (parser == 0) ? 0U : parser->file_bytes;
}
