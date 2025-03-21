#include "credentials.h"

#define JSMN_HEADER

#include <base64.h>
#include <curl/curl.h>
#include <curl/header.h>
#include <definitions.h>
#include <jsmn.h>
#include <stdlib.h>
#include <string.h>

#include "../curl/helpers.h"
#include "../json/json_helpers.h"
#include "api_fields.h"

static int extract_token(const char *src, char *dst, const char *token) {
  jsmn_parser p;
  jsmntok_t tokens[10];
  int t = 0;

  (void)jsmn_init(&p);

  const int ret = jsmn_parse(&p, src, strlen(src), tokens, sizeof(tokens) / sizeof(jsmntok_t));

  if (eval_jsmn_return(ret)) {
    return 1;
  }

  while (t < ret) {
    size_t token_len = (size_t)(tokens[t + 1].end - tokens[t + 1].start);
    if (jsoneq(src, &tokens[t], token)) {
      memcpy((void *)dst, (void *)(src + tokens[t + 1].start), token_len);
    } else if (jsoneq(src, &tokens[t], "error") || jsoneq(src, &tokens[t], "errorCode")) {
      PRINTERR(
          "Error \"%.*s\": %.*s", tokens[t + 1].end - tokens[t + 1].start,
          src + tokens[t + 1].start, tokens[t + 3].end - tokens[t + 3].start,
          src + tokens[t + 3].start
      );
      return 1;
    }
    t++;
  }
  return 0;
}

int get_vzw_auth_token(const char *vzw_auth_keys, char *vzw_auth_token) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};
  CharBuff response_data = {NULL, 0};
  char auth_token_field[OAUTH2_TOKEN_FIELD_SIZE];

  ptr = stpcpy(auth_token_field, OAUTH2_TOKEN_FIELD);
  (void)base64(vzw_auth_keys, strlen(vzw_auth_keys), ptr);

  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  headers = curl_slist_append(headers, auth_token_field);

  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");
  curl_easy_setopt(curl, CURLOPT_URL, "https://thingspace.verizon.com/api/ts/v1/oauth2/token");
  curl_easy_default_opts(curl, &header_data, &response_data, headers);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    PRINTERR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    goto fail;
  }

  res = extract_token(response_data.response, vzw_auth_token, "access_token");
  if (res != CURLE_OK) {
    PRINTERR("Failed to get VZW Auth Token");
  }

fail:
  free(header_data.response);
  free(response_data.response);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return (int)res;
}

#define USERNAME_FIELD "\"username\":\""
#define PASSWORD_FIELD "\",\"password\":\""

#define LOGIN_FIELD_SIZE(uname, password) \
  sizeof(USERNAME_FIELD) + sizeof(PASSWORD_FIELD) + strlen(uname) + strlen(password)

int get_vzw_m2m_token(
    const char *username, const char *password, const char *vzw_auth_token, char *vzw_m2m_token
) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};
  CharBuff response_data = {NULL, 0};

  char post_field[LOGIN_FIELD_SIZE(username, password)];
  char access_token_field[ACCESS_TOKEN_FIELD_SIZE(vzw_auth_token)];

  ptr = stpcpy(access_token_field, ACCESS_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_auth_token);

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);

  post_field[0] = '{';
  ptr = stpcpy(&post_field[1], USERNAME_FIELD);
  ptr = stpcpy(ptr, username);
  ptr = stpcpy(ptr, PASSWORD_FIELD);
  ptr = stpcpy(ptr, password);
  (void)stpcpy(ptr, "\"}");

  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);
  curl_easy_setopt(curl, CURLOPT_URL, VZW_M2M_REST_API "/session/login");
  curl_easy_default_opts(curl, &header_data, &response_data, headers);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    PRINTERR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    goto fail;
  }

  res = extract_token(response_data.response, vzw_m2m_token, "sessionToken");
  if (res != CURLE_OK) {
    PRINTERR("Failed to get VZW M2M Token");
  }

fail:
  free(header_data.response);
  free(response_data.response);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return res;
}
