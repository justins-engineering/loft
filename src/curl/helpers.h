/** @brief Function definitions for user defined curl callback and helper functions.
 */
#ifndef HELPERS_H
#define HELPERS_H
#include <curl/curl.h>
#include <definitions.h>

/** @fn const size_t stack_mem_write_callback(const char *buffer, const size_t
 * size, size_t nitems, const void *userdata);
 *  @brief User defined curl callback for writing response data to stack mem.
 */
size_t stack_mem_write_callback(
    const char *buffer, const size_t size, const size_t nitems, const void *userdata
);

/** @fn const size_t heap_mem_write_callback(const char *buffer, const size_t
 * size, size_t nitems, const void *userdata);
 *  @brief User defined curl callback for writing response data to heap mem.
 */
size_t heap_mem_write_callback(
    const char *buffer, const size_t size, const size_t nitems, const void *userdata
);

void curl_easy_default_opts(
    CURL *curl, CharBuff *header_data, CharBuff *response_data, struct curl_slist *headers
);
#endif  // HELPERS_H
