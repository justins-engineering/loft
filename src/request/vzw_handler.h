#ifndef VZW_HANDLER_H
#define VZW_HANDLER_H

#include <nxt_unit_typedefs.h>

void vzw_registered_callback_listeners(nxt_unit_request_info_t *req_info, int rc);

void vzw_send_nidd(nxt_unit_request_info_t *req_info, int rc);

void vzw_callback(nxt_unit_request_info_t *req_info, int rc);

#endif  // VZW_HANDLER_H
