// #include <hiredis/adapters/libevent.h>
// #include <hiredis/async.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

// void *redisCommand(redisContext *c, const char *format, ...);
// void freeReplyObject(void *reply);

redisContext *redis_connect(void) {
  redisContext *c = redisConnect("172.17.0.1", 6379);
  if (c == NULL || c->err) {
    if (c) {
      PRINTERR("Error: %s", c->errstr);
      // handle error
    } else {
      PRINTERR("Can't allocate redis context");
    }
  }
  return c;
}

int redis_set(redisContext *context, char *key, char *value) {
  redisReply *reply;

  reply = redisCommand(context, "SET key:%s %s", key, value);
  if (reply == NULL || context->err) {
    PRINTERR("Error:  Couldn't execute redisCommand SET");
    return 1;
  }

  PRINTSUCCES("SET: %s", reply->str);
  freeReplyObject(reply);
  return 0;
}

int redis_get(redisContext *context, char *key) {
  redisReply *reply;

  reply = redisCommand(context, "GET key:%s", key);
  if (reply == NULL || context->err) {
    PRINTERR("Error:  Couldn't execute redisCommand GET");
    return 1;
  }

  PRINTSUCCES("GET %s: %s", key, reply->str);
  freeReplyObject(reply);
  return 0;
}
