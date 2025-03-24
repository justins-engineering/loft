
/*
 * Copyright (C) NGINX, Inc.
 */

#include <curl/curl.h>
#include <nxt_unit.h>
#include <pthread.h>
#include <stdlib.h>

#include "request/router.h"

static int thread_count;
static pthread_t *threads = NULL;

static void *worker(void *main_ctx) {
  int rc;
  nxt_unit_ctx_t *ctx;

  ctx = nxt_unit_ctx_alloc(main_ctx, NULL);
  if (ctx == NULL) {
    return (void *)(intptr_t)NXT_UNIT_ERROR;
  }

  nxt_unit_log(ctx, NXT_UNIT_LOG_INFO, "Starting worker");
  rc = nxt_unit_run(ctx);

  nxt_unit_done(ctx);

  return (void *)(intptr_t)rc;
}

static int ready_handler(nxt_unit_ctx_t *ctx) {
  int i, err;

  nxt_unit_debug(ctx, "ready");

  if (thread_count <= 1) {
    return NXT_UNIT_OK;
  }

  threads = nxt_unit_malloc(ctx, sizeof(pthread_t) * (thread_count - 1));
  if (threads == NULL) {
    return NXT_UNIT_ERROR;
  }

  for (i = 0; i < thread_count - 1; i++) {
    err = pthread_create(&threads[i], NULL, worker, ctx);
    if (err != 0) {
      return NXT_UNIT_ERROR;
    }
  }

  return NXT_UNIT_OK;
}

int main(int argc, char **argv) {
  int i, err;
  void *thread_ret;
  nxt_unit_ctx_t *ctx;
  nxt_unit_init_t init;

  curl_global_init(CURL_GLOBAL_NOTHING);

  if (argc == 3 && strcmp(argv[1], "-t") == 0) {
    thread_count = atoi(argv[2]);
  }

  memset(&init, 0, sizeof(nxt_unit_init_t));

  init.callbacks.request_handler = request_router;
  init.callbacks.ready_handler = ready_handler;

  ctx = nxt_unit_init(&init);
  if (ctx == NULL) {
    return NXT_UNIT_ERROR;
  }

  err = nxt_unit_run(ctx);

  nxt_unit_debug(ctx, "main worker finished with %d code", err);

  if ((threads != NULL) && (thread_count > 1)) {
    for (i = 0; i < thread_count - 1; i++) {
      err = pthread_join(threads[i], &thread_ret);

      if (!err) {
        nxt_unit_debug(ctx, "join thread #%d", i);

      } else {
        nxt_unit_alert(
            ctx, "pthread_join(#%d) failed: %s (%d)", i, strerror(err), err
        );
      }

      nxt_unit_log(
          ctx, NXT_UNIT_LOG_INFO, "Worker exited with code %" PRIiPTR,
          (intptr_t)thread_ret
      );
    }
    nxt_unit_free(ctx, threads);
  }

  curl_global_cleanup();
  nxt_unit_done(ctx);
  nxt_unit_debug(NULL, "main worker done");

  return NXT_UNIT_OK;
}
