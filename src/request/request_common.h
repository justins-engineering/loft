#ifndef REQUEST_COMMON_H
#define REQUEST_COMMON_H
#include <nxt_unit.h>

#define CONTENT_TYPE "Content-Type"
#define TEXT_HTML_UTF8 "text/html; charset=utf-8"
#define TEXT_PLAIN_UTF8 "text/plain; charset=utf-8"
#define JSON_UTF8 "application/json; charset=utf-8"
#define OCTET_STREAM "application/octet-stream;"

int response_init(nxt_unit_request_info_t *req_info, int rc, uint16_t status, const char *type);

#endif  // REQUEST_COMMON_H
