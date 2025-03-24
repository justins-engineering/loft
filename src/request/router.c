/** @headerfile request_handler.h */
#include "router.h"

#include <definitions.h>
#include <nxt_unit_request.h>
#include <nxt_unit_sptr.h>
#include <string.h>

#include "firmware_handler.h"
#include "request_common.h"
#include "vzw_handler.h"

void request_router(nxt_unit_request_info_t *req_info) {
  int rc = 0;
  nxt_unit_sptr_t *rp = &req_info->request->path;

  char *path = nxt_unit_sptr_get(rp);

  if (strncmp(path, "/vzw", 4) == 0) {
    if ((strncmp(path + 4, "/nidd", 5) == 0)) {
      vzw_send_nidd(req_info, rc);
    } else if (strncmp(path + 4, "/registered_callback_listeners", 30) == 0) {
      vzw_registered_callback_listeners(req_info, rc);

    } else {
      vzw_callback(req_info, rc);
    }
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
