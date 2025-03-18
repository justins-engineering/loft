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
#include "curl_callbacks.h"
#include "json_helpers.h"

#define CONTENT_TYPE_JSON "Content-Type:application/json"
#define ACCEPT_JSON "Accept:application/json"

static int extract_token(const char *src, char *dst, const char *token) {
  jsmn_parser p;
  jsmntok_t tokens[10];
  int t = 0;

  (void)jsmn_init(&p);

  const int ret = jsmn_parse(
      &p, src, strlen(src), tokens, sizeof(tokens) / sizeof(jsmntok_t)
  );

  if (eval_jsmn_return(ret)) {
    return 1;
  }

  while (t < ret) {
    size_t token_len = (size_t)(tokens[t + 1].end - tokens[t + 1].start);
    if (jsoneq(src, &tokens[t], token)) {
      (void)memcpy((void *)dst, (void *)(src + tokens[t + 1].start), token_len);
    } else if (jsoneq(src, &tokens[t], "error") ||
        jsoneq(src, &tokens[t], "errorCode")) {
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

/** Base64 encoding is deterministic, so we know the length ahead of time + 1
 * for null terminator */
#define ENC_LEN BASE64LEN(strlen(vzw_auth_keys)) + 1

/** OAUTH2_TOKEN_FIELD length is 21 */
#define OAUTH2_TOKEN_FIELD_SIZE strlen(OAUTH2_TOKEN_FIELD) + ENC_LEN

int get_vzw_auth_token(const char *vzw_auth_keys, char *vzw_auth_token) {
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};
  RecvData response_data = {NULL, 0};

  char encoded_token[ENC_LEN];
  char auth_token_field[OAUTH2_TOKEN_FIELD_SIZE];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  curl_easy_setopt(
      curl, CURLOPT_URL, "https://thingspace.verizon.com/api/ts/v1/oauth2/token"
  );

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);

  int base64StringLen =
      base64(vzw_auth_keys, strlen(vzw_auth_keys), encoded_token);

  (void)memcpy(auth_token_field, OAUTH2_TOKEN_FIELD, strlen(OAUTH2_TOKEN_FIELD));
  (void)memcpy(&auth_token_field[strlen(OAUTH2_TOKEN_FIELD)], encoded_token, base64StringLen);
  auth_token_field[OAUTH2_TOKEN_FIELD_SIZE - 1] = 0;

  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded"
  );
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

/** USERNAME_FIELD + PASSWORD_FIELD length + 4 for {"}0 */
#define LOGIN_FIELD_SIZE \
  sizeof(USERNAME_FIELD) + sizeof(PASSWORD_FIELD) + sizeof(VZW_USERNAME) + sizeof(VZW_PASSWORD)

int get_vzw_m2m_token(
    const char *username, const char *password, const char *vzw_auth_token,
    char *vzw_m2m_token
) {
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};
  RecvData response_data = {NULL, 0};

  char post_field[LOGIN_FIELD_SIZE];
  size_t access_token_field_size = strlen(ACCESS_TOKEN_FIELD) + strlen(vzw_auth_token) + 1;
  char access_token_field[access_token_field_size];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

  (void)memcpy(access_token_field, ACCESS_TOKEN_FIELD, strlen(ACCESS_TOKEN_FIELD));
  (void
  )memcpy(&access_token_field[strlen(ACCESS_TOKEN_FIELD)], vzw_auth_token, strlen(vzw_auth_token));
  access_token_field[access_token_field_size - 1] = 0;

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(
      curl, CURLOPT_URL,
      "https://thingspace.verizon.com/api/m2m/v1/session/login"
  );

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heap_mem_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  post_field[0] = '{';
  (void)memcpy(&post_field[1], USERNAME_FIELD, strlen(USERNAME_FIELD));
  (void
  )memcpy(&post_field[strlen(USERNAME_FIELD) + 1], username, strlen(username));
  (void)memcpy(
      &post_field[strlen(USERNAME_FIELD) + 1 + strlen(username)],
      PASSWORD_FIELD, strlen(PASSWORD_FIELD)
  );
  (void)memcpy(
      &post_field
          [strlen(USERNAME_FIELD) + 1 + strlen(username) +
           strlen(PASSWORD_FIELD)],
      password, strlen(password)
  );
  post_field[LOGIN_FIELD_SIZE - 3] = '"';
  post_field[LOGIN_FIELD_SIZE - 2] = '}';
  post_field[LOGIN_FIELD_SIZE - 1] = 0;
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
      sizeof(MDT_FIELD) + sizeof(MESSAGE_FIELD) + MDN_SIZE + MAX_MDT_SIZE + MAX_MESSAGE_SIZE + 6

int send_nidd_data(char *vzw_auth_token, char *vzw_m2m_token, char *mdn, char *mdt, char *message) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  RecvData header_data = {NULL, 0};
  RecvData response_data = {NULL, 0};

  if (BASE64LEN(strlen(message)) > MAX_MESSAGE_SIZE) {
    PRINTERR("NIDD message length too large");
    return 1;
  }

  char post_field[SEND_NIDD_BODY_MAX_SIZE];

  size_t access_token_field_size = strlen(ACCESS_TOKEN_FIELD) + strlen(vzw_auth_token) + 1;
  char access_token_field[access_token_field_size];

  size_t session_token_field_size = strlen(SESSION_TOKEN_FIELD) + strlen(vzw_m2m_token) + 1;
  char session_token_field[session_token_field_size];

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

  (void)memcpy(access_token_field, ACCESS_TOKEN_FIELD, strlen(ACCESS_TOKEN_FIELD));
  (void
  )memcpy(&access_token_field[strlen(ACCESS_TOKEN_FIELD)], vzw_auth_token, strlen(vzw_auth_token));
  access_token_field[access_token_field_size - 1] = 0;

  (void)memcpy(session_token_field, SESSION_TOKEN_FIELD, strlen(ACCESS_TOKEN_FIELD));
  (void
  )memcpy(&session_token_field[strlen(SESSION_TOKEN_FIELD)], vzw_m2m_token, strlen(vzw_m2m_token));
  session_token_field[session_token_field_size - 1] = 0;

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
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

  // char post_field[] = SEND_NIDD_BODY(mdn, mdt, message);
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

  PRINTDBG("%s", post_field);

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    PRINTERR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    goto fail;
  }

  PRINTDBG("%s", response_data.response);

fail:
  free(header_data.response);
  free(response_data.response);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return res;
}
