/** @headerfile vzw_connect.h */
#include "vzw_connect.h"

#define JSMN_HEADER

#include <base64.h>
#include <config.h>
#include <curl/curl.h>
#include <curl/header.h>
#include <jsmn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config/vzw_secrets.h"
#include "../json/json_helpers.h"

#define CONTENT_TYPE_JSON "Content-Type:application/json"
#define ACCEPT_JSON "Accept:application/json"

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

#define OAUTH2_TOKEN_FIELD "Authorization:Basic "

/** Base64 encoding is deterministic, so we know the length ahead of time */
#define OAUTH2_TOKEN_FIELD_SIZE sizeof(OAUTH2_TOKEN_FIELD) + BASE64LEN(strlen(vzw_auth_keys))

int get_vzw_auth_token(const char *vzw_auth_keys, char *vzw_auth_token) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};
  RecvData response_data = {NULL, 0};
  char auth_token_field[OAUTH2_TOKEN_FIELD_SIZE];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  curl_easy_setopt(curl, CURLOPT_URL, "https://thingspace.verizon.com/api/ts/v1/oauth2/token");

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);

  ptr = stpcpy(auth_token_field, OAUTH2_TOKEN_FIELD);
  (void)base64(vzw_auth_keys, strlen(vzw_auth_keys), ptr);

  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  headers = curl_slist_append(headers, auth_token_field);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");

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
#define ACCESS_TOKEN_FIELD "Authorization:Bearer "
#define ACCESS_TOKEN_FIELD_SIZE(t) sizeof(ACCESS_TOKEN_FIELD) + strlen(t)
#define LOGIN_FIELD_SIZE \
  sizeof(USERNAME_FIELD) + sizeof(PASSWORD_FIELD) + sizeof(VZW_USERNAME) + sizeof(VZW_PASSWORD)

int get_vzw_m2m_token(
    const char *username, const char *password, const char *vzw_auth_token, char *vzw_m2m_token
) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};
  RecvData response_data = {NULL, 0};

  char post_field[LOGIN_FIELD_SIZE];
  char access_token_field[ACCESS_TOKEN_FIELD_SIZE(vzw_auth_token)];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  ptr = stpcpy(access_token_field, ACCESS_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_auth_token);

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_URL, "https://thingspace.verizon.com/api/m2m/v1/session/login");

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  post_field[0] = '{';
  ptr = stpcpy(&post_field[1], USERNAME_FIELD);
  ptr = stpcpy(ptr, username);
  ptr = stpcpy(ptr, PASSWORD_FIELD);
  ptr = stpcpy(ptr, password);
  (void)stpcpy(ptr, "\"}");

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);

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

#define SESSION_TOKEN_FIELD "VZ-M2M-Token:"
#define SESSION_TOKEN_FIELD_SIZE(t) sizeof(SESSION_TOKEN_FIELD) + strlen(t)

#define DEVICE_ID_FIELD_START "\"deviceIds\":[{\"id\":\""
#define DEVICE_ID_FIELD_END "\",\"kind\":\"MDN\"}],"

#define ACCOUNT_NAME_FIELD "\"accountName\":\"" VZW_ACCOUNT_NAME "\","
#define MDT_FIELD "\"maximumDeliveryTime\":\""
#define MESSAGE_FIELD "\"message\":\""

#define MDN_SIZE 10
#define MAX_MDT_SIZE 7
#define MAX_MESSAGE_SIZE 10864

#define SEND_NIDD_BODY_MAX_SIZE                                                              \
  sizeof(DEVICE_ID_FIELD_START) + sizeof(DEVICE_ID_FIELD_END) + sizeof(ACCOUNT_NAME_FIELD) + \
      sizeof(MDT_FIELD) + sizeof(MESSAGE_FIELD) + MDN_SIZE + MAX_MDT_SIZE + MAX_MESSAGE_SIZE

int send_nidd_data(
    char *vzw_auth_token, char *vzw_m2m_token, char *mdn, char *mdt, char *message,
    RecvData *response_data
) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};

  if (BASE64LEN(strlen(message)) > MAX_MESSAGE_SIZE) {
    PRINTERR("NIDD message length too large");
    return 1;
  }

  char post_field[SEND_NIDD_BODY_MAX_SIZE];
  char access_token_field[ACCESS_TOKEN_FIELD_SIZE(vzw_auth_token)];
  char session_token_field[SESSION_TOKEN_FIELD_SIZE(vzw_m2m_token)];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  ptr = stpcpy(access_token_field, ACCESS_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_auth_token);

  ptr = stpcpy(session_token_field, SESSION_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_m2m_token);

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);
  headers = curl_slist_append(headers, session_token_field);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(
      curl, CURLOPT_URL, "https://thingspace.verizon.com/api/m2m/v1/devices/nidd/message"
  );

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data);

  post_field[0] = '{';
  ptr = stpcpy(&post_field[1], DEVICE_ID_FIELD_START);
  ptr = stpcpy(ptr, mdn);
  ptr = stpcpy(ptr, DEVICE_ID_FIELD_END);
  ptr = stpcpy(ptr, ACCOUNT_NAME_FIELD);
  ptr = stpcpy(ptr, MDT_FIELD);
  ptr = stpcpy(ptr, mdt);
  ptr = stpcpy(ptr, "\",");
  ptr = stpcpy(ptr, MESSAGE_FIELD);
  ptr += base64(message, strlen(message), ptr);
  (void)stpcpy(ptr, "\"}");

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    PRINTERR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    goto fail;
  }

fail:
  free(header_data.response);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return res;
}
