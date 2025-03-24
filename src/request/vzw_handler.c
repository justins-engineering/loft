/** @headerfile request_handler.h */
#include "vzw_handler.h"

#include <definitions.h>
#include <nxt_unit.h>
#include <nxt_unit_request.h>
#include <nxt_unit_sptr.h>
#include <stdlib.h>
#include <vzw_nidd.h>

#include "../config/vzw_secrets.h"
#include "../db/redis_connector.h"
#include "request_common.h"

#define CharBuff VZWResponseData

#define REDIS_VZW_AUTH_KEY "VZW_AUTH_TOKEN"
#define REDIS_VZW_M2M_KEY "VZW_SESSION_TOKEN"

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

void vzw_registered_callback_listeners(nxt_unit_request_info_t *req_info, int rc) {
  nxt_unit_buf_t *buf;
  redisContext *context = redis_connect();
  CharBuff response_data = {NULL, 0};
  char vzw_auth_token[50];
  char vzw_session_token[50];

  char *http_method = nxt_unit_sptr_get(&req_info->request->method);

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
  (void)redisFree(context);
  nxt_unit_request_done(req_info, rc);
}

void vzw_send_nidd(nxt_unit_request_info_t *req_info, int rc) {
  nxt_unit_buf_t *buf;
  redisContext *context = redis_connect();
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
  (void)redisFree(context);
  nxt_unit_request_done(req_info, rc);
}

void vzw_callback(nxt_unit_request_info_t *req_info, int rc) {
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
