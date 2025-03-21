#include "helpers.h"

#include <stdlib.h>
#include <string.h>

size_t stack_mem_write_callback(
    const char *buffer, const size_t size, const size_t nitems, const void *userdata
) {
  size_t realsize = size * nitems;
  CharBuff *mem = (CharBuff *)userdata;

  memcpy(&(mem->response[mem->size]), buffer, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
  return realsize;
}

size_t heap_mem_write_callback(
    const char *buffer, const size_t size, const size_t nitems, const void *userdata
) {
  size_t realsize = size * nitems;
  CharBuff *mem = (CharBuff *)userdata;

  char *res = realloc(mem->response, mem->size + realsize + 1);
  if (res == NULL) {
    return 0;
  }

  mem->response = res;

  (void)memcpy(&(mem->response[mem->size]), buffer, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
  return realsize;
}

void curl_easy_default_opts(
    CURL *curl, CharBuff *header_data, CharBuff *response_data, struct curl_slist *headers
) {
  //** NOSIGNAL flag required if multithereaded */
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, header_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}
