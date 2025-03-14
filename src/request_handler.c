/** @headerfile request_handler.h */
#include "request_handler.h"

#include <nxt_unit_request.h>
#include <nxt_unit_typedefs.h>

#define JSMN_HEADER

#include <config.h>
#include <nxt_clang.h>
#include <nxt_unit.h>
#include <nxt_unit_sptr.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../config/vzw_secrets.h"
#include "firmware_requests.h"
#include "redis_connect.h"
#include "vzw_connect.h"

#define CONTENT_TYPE "Content-Type"
#define TEXT_HTML_UTF8 "text/html; charset=utf-8"
#define TEXT_PLAIN_UTF8 "text/plain; charset=utf-8"
#define JSON_UTF8 "application/json; charset=utf-8"
#define OCTET_STREAM "application/octet-stream;"

static inline char *copy(char *p, const void *src, uint32_t len) {
  memcpy(p, src, len);
  return p + len;
}

static int get_vzw_tokens(char *vzw_auth_token, char *vzw_m2m_token) {
  int rc = 0;

  rc = get_vzw_auth_token(VZW_PUBLIC_KEY ":" VZW_PRIVATE_KEY, vzw_auth_token);
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    goto fail;
  }
  PRINTDBG("auth token: %s", vzw_auth_token);

  rc = get_vzw_m2m_token(
      VZW_USERNAME, VZW_PASSWORD, vzw_auth_token, vzw_m2m_token
  );
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    goto fail;
  }
  PRINTDBG("m2m token: %s", vzw_m2m_token);

fail:
  return rc;
}

static int response_init(
    nxt_unit_request_info_t *req_info, int rc, uint16_t status, const char *type
) {
  rc = nxt_unit_response_init(
      req_info, status, 1, nxt_length(CONTENT_TYPE) + strlen(type)
  );
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to initialize response");
    return 1;
  }

  rc = nxt_unit_response_add_field(
      req_info, CONTENT_TYPE, nxt_length(CONTENT_TYPE), type, strlen(type)
  );
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to add field to response");
    return 1;
  }

  rc = nxt_unit_response_send(req_info);
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to send response headers");
    return 1;
  }

  return 0;
}

void vzw_request_handler(nxt_unit_request_info_t *req_info, int rc) {
  char *p;
  nxt_unit_buf_t *buf;

  char vzw_auth_token[50] = "\0";
  char vzw_m2m_token[50] = "\0";

  rc = response_init(req_info, rc, 200, TEXT_PLAIN_UTF8);
  if (rc == 1) {
    goto fail;
  }

  rc = get_vzw_tokens(&vzw_auth_token[0], &vzw_m2m_token[0]);
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to get VZW credentials");
    goto fail;
  }

  buf = nxt_unit_response_buf_alloc(
      req_info, ((req_info->request_buf->end - req_info->request_buf->start) +
                 strlen("Hello world!\n"))
  );
  if (nxt_slow_path(buf == NULL)) {
    rc = NXT_UNIT_ERROR;
    nxt_unit_req_error(req_info, "Failed to allocate response buffer");
    goto fail;
  }

  p = buf->free;

  p = copy(p, "Hello world!", strlen("Hello world!"));
  *p++ = '\n';

  buf->free = p;
  rc = nxt_unit_buf_send(buf);
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to send buffer");
    goto fail;
  }

fail:
  nxt_unit_request_done(req_info, rc);
}

void firmware_request_handler(nxt_unit_request_info_t *req_info, int rc) {
  char *p;
  nxt_unit_buf_t *buf;
  struct stat file_info;
  FILE *fptr;
  size_t io_blocks;

  rc = response_init(req_info, rc, 200, OCTET_STREAM);
  if (rc == 1) {
    goto fail;
  }

  rc = download_firmware_github(&fptr);
  if (nxt_slow_path(rc != NXT_UNIT_OK)) {
    nxt_unit_req_error(req_info, "Failed to get latest firmware from GitHub");
    goto fail;
  }

  if (fstat(fileno(fptr), &file_info) != 0) {
    perror("[firmware_request_handler] Failed to stat firmware file. Error");
    goto fail;
  }

  /* file_info.st_blocks is the number of 512B blocks allocated on Linux.
   * file_info.st_blksize is the preferred blocksize for file system I/O, likey
   * larger that 512B.
   */
  if (file_info.st_blksize > 512) {
    io_blocks = file_info.st_blocks / (file_info.st_blksize / 512);
  } else if (file_info.st_blksize < 512) {
    io_blocks = file_info.st_blocks * (512 / file_info.st_blksize);
  } else {
    io_blocks = file_info.st_blocks;
  }

  PRINTDBG("Firmware file size: %ld B", file_info.st_size);
  PRINTDBG("I/O block size: %ld B", file_info.st_blksize);
  PRINTDBG("Blocks allocated: %ld", io_blocks);

  PRINTDBG("Sending firmware...");
  for (size_t i = 0; i < io_blocks; i++) {
    buf = nxt_unit_response_buf_alloc(req_info, (file_info.st_blksize));
    if (nxt_slow_path(buf == NULL)) {
      rc = NXT_UNIT_ERROR;
      PRINTERR("Failed to allocate response buffer");
      goto fail;
    }
    p = buf->free;

    if (fread(p, file_info.st_blksize, 1, fptr) == 1) {
      buf->free = (p + file_info.st_blksize);
    } else if (ferror(fptr)) {
      perror("Failed to read firmware file. Error");
      goto fail;
    } else {
      /* Reached EOF, we're on the last buffer. The remaining file data is
       * smaller than the allocated memory and we don't want to send extra data.
       * To get the actual size of data in this buffer we have to do some math:
       * The size of total bytes alocated - actual file size bytes gives us the
       * size of unused memory in bytes. We subtract that from the size of the
       * current buffer, file_info.st_blksize.
       */
      buf->free =
          (p + (file_info.st_blksize -
                ((file_info.st_blocks * 512) - file_info.st_size)));
    }

    rc = nxt_unit_buf_send(buf);
    if (nxt_slow_path(rc != NXT_UNIT_OK)) {
      PRINTERR("Failed to send buffer");
      goto fail;
    }
  }
  PRINTDBG("Done");

fail:
  if (fclose(fptr)) {
    perror("Failed to close firmware file. Error");
  }
  nxt_unit_request_done(req_info, rc);
}

void redis_request_handler(nxt_unit_request_info_t *req_info, int rc) {
  rc = response_init(req_info, rc, 200, TEXT_PLAIN_UTF8);
  if (rc == 1) {
    goto fail;
  }

  redisContext *c = redis_connect();

  rc = redis_set(c, (char *)"hiredis", (char *)"its_alive");
  if (rc == 1) {
    goto fail;
  }

  rc = redis_get(c, (char *)"hiredis");

fail:
  nxt_unit_request_done(req_info, rc);
}

void request_router(nxt_unit_request_info_t *req_info) {
  int rc = 0;
  nxt_unit_sptr_t *rp = &req_info->request->path;

  void *path = nxt_unit_sptr_get(rp);

  if (strncmp(path, "/vzw", 4) == 0) {
    (void)vzw_request_handler(req_info, rc);
  } else if ((strncmp(path, "/firmware", 9) == 0)) {
    (void)firmware_request_handler(req_info, rc);
  } else if ((strncmp(path, "/db", 3) == 0)) {
    (void)redis_request_handler(req_info, rc);
  } else {
    response_init(req_info, rc, 404, TEXT_PLAIN_UTF8);
    if (rc == 1) {
      goto fail;
    }
    nxt_unit_response_write(req_info, "Error 404", sizeof("Error 404") - 1);
  }

fail:
  nxt_unit_request_done(req_info, rc);
}
