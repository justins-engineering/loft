/** @brief Macro and function defines for the hiredis.
 */

#ifndef REDIS_CONNECT_H
#define REDIS_CONNECT_H

#include <hiredis/hiredis.h>

redisContext *redis_connect(void);
int redis_set(redisContext *context, const char *key, char *value);
int redis_set_ex(redisContext *context, const char *key, char *value, const char *ttl);
int redis_get(redisContext *context, const char *key, char *value);
#endif
