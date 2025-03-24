/** @headerfile request_handler.h */
#include "request_handler.h"

#include <nxt_unit_request.h>
#include <nxt_unit_typedefs.h>

#define JSMN_HEADER

#include <definitions.h>
#include <nxt_unit.h>
#include <nxt_unit_sptr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vzw_nidd.h>

#include "../config/vzw_secrets.h"
#include "db/redis_connect.h"
#include "firmware/firmware_requests.h"

#define CharBuff VZWResponseData

#define CONTENT_TYPE "Content-Type"
#define TEXT_HTML_UTF8 "text/html; charset=utf-8"
#define TEXT_PLAIN_UTF8 "text/plain; charset=utf-8"
#define JSON_UTF8 "application/json; charset=utf-8"
#define OCTET_STREAM "application/octet-stream;"

#define REDIS_VZW_AUTH_KEY "VZW_AUTH_TOKEN"
#define REDIS_VZW_M2M_KEY "VZW_SESSION_TOKEN"

static inline char *copy(char *p, const void *src, uint32_t len) {
  memcpy(p, src, len);
  return p + len;
}

static int response_init(
    nxt_unit_request_info_t *req_info, int rc, uint16_t status, const char *type
) {
  rc = nxt_unit_response_init(req_info, status, 1, sizeof(CONTENT_TYPE) - 1 + strlen(type));
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to initialize response");
    return 1;
  }

  rc = nxt_unit_response_add_field(
      req_info, CONTENT_TYPE, sizeof(CONTENT_TYPE) - 1, type, strlen(type)
  );
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to add field to response");
    return 1;
  }

  rc = nxt_unit_response_send(req_info);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to send response headers");
    return 1;
  }

  return 0;
}

static int vzw_credentials_handler(
    redisContext *context, char *vzw_auth_token, char *vzw_session_token
) {
  int rc = redis_get(context, REDIS_VZW_AUTH_KEY, vzw_auth_token);
  if (rc) {
    return 1;
  }

  if (*vzw_auth_token == '\0') {
    rc = vzw_get_auth_token(VZW_PUBLIC_KEY ":" VZW_PRIVATE_KEY, vzw_auth_token);
    if (rc) {
      return 1;
    }

    rc = redis_set_ex(context, REDIS_VZW_AUTH_KEY, vzw_auth_token, "3600");
    if (rc) {
      return 1;
    }
  }

  rc = redis_get(context, REDIS_VZW_M2M_KEY, vzw_session_token);
  if (rc) {
    return 1;
  }

  if (*vzw_session_token == '\0') {
    rc = vzw_get_session_token(VZW_USERNAME, VZW_PASSWORD, vzw_auth_token, vzw_session_token);
    if (rc) {
      return 1;
    }

    rc = redis_set_ex(context, REDIS_VZW_M2M_KEY, vzw_session_token, "1200");
    if (rc) {
      return 1;
    }
  }

  return 0;
}

void vzw_registered_callback_listeners(
    nxt_unit_request_info_t *req_info, int rc, redisContext *context
) {
  nxt_unit_buf_t *buf;
  CharBuff response_data = {NULL, 0};
  char vzw_auth_token[50];
  char vzw_session_token[50];

  char *http_method = nxt_unit_sptr_get(&req_info->request->method);
  PRINTDBG("method: %s", http_method);

  rc = vzw_credentials_handler(context, vzw_auth_token, vzw_session_token);
  if (rc == 1) {
    goto end;
  }

  switch (http_method[0]) {
    case 'G':
      rc = vzw_get_registered_callback_listeners(
          VZW_ACCOUNT_NAME, vzw_auth_token, vzw_session_token, &response_data
      );
      if (rc == 1) {
        goto end;
      }
      break;
    case 'D':
      rc = vzw_delete_registered_callback_listeners(
          VZW_ACCOUNT_NAME, vzw_auth_token, vzw_session_token, &response_data, (char *)"NiddService"
      );
      if (rc == 1) {
        goto end;
      }
      break;
    case 'P':
      if (http_method[1] == 'O') {
        rc = vzw_set_registered_callback_listeners(
            VZW_ACCOUNT_NAME, vzw_auth_token, vzw_session_token, &response_data,
            (char *)"NiddService", (char *)""
        );
        if (rc == 1) {
          goto end;
        }
        break;
      }
      // fall through
    default:
      rc = nxt_unit_response_init(req_info, 405, 0, 0);
      if (rc) {
        nxt_unit_req_error(req_info, "Failed to initialize response");
      }

      rc = nxt_unit_response_send(req_info);
      if (rc) {
        nxt_unit_req_error(req_info, "Failed to send response headers");
      }
      goto end;
  }

  rc = response_init(req_info, rc, 202, JSON_UTF8);
  if (rc == 1) {
    goto end;
  }

  buf = nxt_unit_response_buf_alloc(
      req_info, ((req_info->request_buf->end - req_info->request_buf->start) + response_data.size)
  );
  if (buf == NULL) {
    rc = NXT_UNIT_ERROR;
    nxt_unit_req_error(req_info, "Failed to allocate response buffer");
    goto end;
  }

  buf->free = mempcpy(buf->free, response_data.response, response_data.size);

  rc = nxt_unit_buf_send(buf);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to send buffer");
    goto end;
  }

end:
  free(response_data.response);
  nxt_unit_request_done(req_info, rc);
}

void vzw_send_nidd(nxt_unit_request_info_t *req_info, int rc, redisContext *context) {
  nxt_unit_buf_t *buf;
  CharBuff response_data = {NULL, 0};

  rc = response_init(req_info, rc, 202, JSON_UTF8);
  if (rc == 1) {
    goto fail;
  }

  char vzw_auth_token[50];
  char vzw_session_token[50];

  rc = vzw_credentials_handler(context, vzw_auth_token, vzw_session_token);
  if (rc == 1) {
    goto fail;
  }

  rc = vzw_send_nidd_data(
      VZW_ACCOUNT_NAME, vzw_auth_token, vzw_session_token, (char *)TEST_MDN, (char *)"400",
      (char *)"Hello world!\n", &response_data
  );
  if (rc == 1) {
    goto fail;
  }

  buf = nxt_unit_response_buf_alloc(
      req_info, ((req_info->request_buf->end - req_info->request_buf->start) + response_data.size)
  );
  if (buf == NULL) {
    rc = NXT_UNIT_ERROR;
    nxt_unit_req_error(req_info, "Failed to allocate response buffer");
    goto fail;
  }

  buf->free = mempcpy(buf->free, response_data.response, response_data.size);

  rc = nxt_unit_buf_send(buf);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to send buffer");
    goto fail;
  }

fail:
  free(response_data.response);
  nxt_unit_request_done(req_info, rc);
}

void vzw_callback(nxt_unit_request_info_t *req_info, int rc, redisContext *context) {
  uint64_t buf_len;
  char *read_buf;

  rc = nxt_unit_response_init(req_info, 204, 0, 0);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to initialize response");
  }

  rc = nxt_unit_response_send(req_info);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to send response headers");
  }

  buf_len = req_info->request->content_length;
  read_buf = malloc(buf_len);
  (void)nxt_unit_request_read(req_info, read_buf, buf_len);

  nxt_unit_request_done(req_info, rc);
  PRINTDBG("\n%s\n", read_buf);
  free(read_buf);
}

static void firmware_request_handler(nxt_unit_request_info_t *req_info, int rc) {
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
  if (rc) {
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
    if (buf == NULL) {
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
    if (rc) {
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

void request_router(nxt_unit_request_info_t *req_info) {
  int rc = 0;
  nxt_unit_sptr_t *rp = &req_info->request->path;

  char *path = nxt_unit_sptr_get(rp);

  if (strncmp(path, "/vzw", 4) == 0) {
    redisContext *c = redis_connect();
    if ((strncmp(path + 4, "/nidd", 5) == 0)) {
      (void)vzw_send_nidd(req_info, rc, c);
    } else if (strncmp(path + 4, "/registered_callback_listeners", 30) == 0) {
      vzw_registered_callback_listeners(req_info, rc, c);

    } else {
      (void)vzw_callback(req_info, rc, c);
    }
    (void)redisFree(c);
    goto end;
  } else if ((strncmp(path, "/firmware", 9) == 0)) {
    (void)firmware_request_handler(req_info, rc);
    goto end;
  }

  response_init(req_info, rc, 404, TEXT_PLAIN_UTF8);
  if (rc == 1) {
    goto end;
  }
  nxt_unit_response_write(req_info, "Error 404", sizeof("Error 404") - 1);

end:
  nxt_unit_request_done(req_info, rc);
}
