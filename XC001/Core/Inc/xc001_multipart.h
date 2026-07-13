#ifndef XC001_MULTIPART_H
#define XC001_MULTIPART_H

#include <stddef.h>
#include <stdint.h>

#define XC001_MULTIPART_BOUNDARY_MAX 96U
#define XC001_MULTIPART_LINE_MAX     256U
#define XC001_MULTIPART_FILENAME_MAX 96U

typedef uint8_t (*XC001_MultipartFileBegin)(void *ctx, const char *filename);
typedef uint8_t (*XC001_MultipartFileData)(void *ctx, const uint8_t *data,
                                           uint32_t length);
typedef void (*XC001_MultipartFileEnd)(void *ctx);

typedef struct
{
  XC001_MultipartFileBegin on_file_begin;
  XC001_MultipartFileData on_file_data;
  XC001_MultipartFileEnd on_file_end;
  void *ctx;
} XC001_MultipartCallbacks;

typedef struct
{
  char boundary_line[XC001_MULTIPART_BOUNDARY_MAX + 3U];
  char delimiter[XC001_MULTIPART_BOUNDARY_MAX + 5U];
  uint8_t pending[XC001_MULTIPART_BOUNDARY_MAX + 5U];
  char line[XC001_MULTIPART_LINE_MAX];
  char filename[XC001_MULTIPART_FILENAME_MAX];
  XC001_MultipartCallbacks cb;
  size_t boundary_line_len;
  size_t delimiter_len;
  size_t pending_len;
  size_t line_len;
  uint32_t file_bytes;
  uint8_t state;
  uint8_t part_is_file;
  uint8_t file_active;
  uint8_t file_seen;
  uint8_t body_done;
  uint8_t failed;
} XC001_MultipartParser;

uint8_t XC001_Multipart_Init(XC001_MultipartParser *parser,
                             const char *boundary,
                             const XC001_MultipartCallbacks *callbacks);
uint8_t XC001_Multipart_Execute(XC001_MultipartParser *parser,
                                const uint8_t *data,
                                uint32_t length);
uint8_t XC001_Multipart_IsDone(const XC001_MultipartParser *parser);
uint8_t XC001_Multipart_SawFile(const XC001_MultipartParser *parser);
uint32_t XC001_Multipart_FileBytes(const XC001_MultipartParser *parser);

#endif
