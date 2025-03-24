#include "request_common.h"

int response_init(nxt_unit_request_info_t *req_info, int rc, uint16_t status, const char *type) {
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
