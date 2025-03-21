/** @headerfile vzw_connect.h */
#include "vzw_connect.h"

#include <base64.h>
#include <curl/curl.h>
#include <curl/header.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../curl/helpers.h"
#include "api_fields.h"

static CURL *registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *header_data,
    CharBuff *response_data, struct curl_slist *headers
) {
  char *ptr;
  CURL *curl = curl_easy_init();

  char url[VZW_CALLBACKS_API_LEN(account_name)];
  char access_token_field[ACCESS_TOKEN_FIELD_SIZE(vzw_auth_token)];
  char session_token_field[SESSION_TOKEN_FIELD_SIZE(vzw_m2m_token)];

  ptr = stpcpy(url, VZW_CALLBACKS_API);
  (void)stpcpy(ptr, account_name);

  ptr = stpcpy(access_token_field, ACCESS_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_auth_token);

  ptr = stpcpy(session_token_field, SESSION_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_m2m_token);

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);
  headers = curl_slist_append(headers, session_token_field);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_default_opts(curl, header_data, response_data, headers);

  return curl;
}

int get_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data
) {
  char *ptr;
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};

  CURL *curl = registered_callback_listeners(
      account_name, vzw_auth_token, vzw_m2m_token, &header_data, response_data, headers
  );

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

int set_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data,
    char *service_name, char *url
) {
  char *ptr;
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};

  char post_field[sizeof("{\"name\":\"\",\"url\":\"\"}") + strlen(service_name) + strlen(url)];

  CURL *curl = registered_callback_listeners(
      account_name, vzw_auth_token, vzw_m2m_token, &header_data, response_data, headers
  );

  post_field[0] = '{';
  ptr = stpcpy(&post_field[1], "\"name\":\"");
  ptr = stpcpy(ptr, service_name);
  ptr = stpcpy(ptr, "\",");
  ptr = stpcpy(ptr, "\"url\":\"");
  ptr = stpcpy(ptr, url);
  (void)stpcpy(ptr, "\"}");

  curl_easy_setopt(curl, CURLOPT_POST, 1L);
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

int delete_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data,
    char *service_name
) {
  char *ptr;
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};

  char url[VZW_CALLBACKS_API_LEN(account_name) + sizeof("/name/") + strlen(service_name) - 1];

  ptr = stpcpy(url, VZW_CALLBACKS_API);
  ptr = stpcpy(ptr, account_name);
  ptr = stpcpy(ptr, "/name/");
  (void)stpcpy(ptr, service_name);

  CURL *curl = registered_callback_listeners(
      account_name, vzw_auth_token, vzw_m2m_token, &header_data, response_data, headers
  );

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

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

#define DEVICE_ID_FIELD_START "\"deviceIds\":[{\"id\":\""
#define DEVICE_ID_FIELD_END "\",\"kind\":\"MDN\"}],"

#define ACCOUNT_NAME_FIELD "\"accountName\":\""
#define MDT_FIELD "\"maximumDeliveryTime\":\""
#define MESSAGE_FIELD "\"message\":\""

#define MDN_SIZE 10
#define MAX_MDT_SIZE 7
#define MAX_MESSAGE_SIZE 10864

#define SEND_NIDD_BODY_MAX_SIZE(aname)                                                       \
  sizeof(DEVICE_ID_FIELD_START) + sizeof(DEVICE_ID_FIELD_END) + sizeof(ACCOUNT_NAME_FIELD) + \
      strlen(aname) + sizeof(MDT_FIELD) + sizeof(MESSAGE_FIELD) + MDN_SIZE + MAX_MDT_SIZE +  \
      MAX_MESSAGE_SIZE + 3

int send_nidd_data(
    const char *aname, char *vzw_auth_token, char *vzw_m2m_token, char *mdn, char *mdt,
    char *message, CharBuff *response_data
) {
  char *ptr;
  CURL *curl = curl_easy_init();
  CURLcode res;
  struct curl_slist *headers = NULL;
  CharBuff header_data = {NULL, 0};

  if (BASE64LEN(strlen(message)) > MAX_MESSAGE_SIZE) {
    PRINTERR("NIDD message length too large");
    return 1;
  }

  char post_field[SEND_NIDD_BODY_MAX_SIZE(aname)];
  char access_token_field[ACCESS_TOKEN_FIELD_SIZE(vzw_auth_token)];
  char session_token_field[SESSION_TOKEN_FIELD_SIZE(vzw_m2m_token)];

  ptr = stpcpy(access_token_field, ACCESS_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_auth_token);

  ptr = stpcpy(session_token_field, SESSION_TOKEN_FIELD);
  (void)stpcpy(ptr, vzw_m2m_token);

  headers = curl_slist_append(headers, CONTENT_TYPE_JSON);
  headers = curl_slist_append(headers, ACCEPT_JSON);
  headers = curl_slist_append(headers, access_token_field);
  headers = curl_slist_append(headers, session_token_field);

  post_field[0] = '{';
  ptr = stpcpy(&post_field[1], DEVICE_ID_FIELD_START);
  ptr = stpcpy(ptr, mdn);
  ptr = stpcpy(ptr, DEVICE_ID_FIELD_END);
  ptr = stpcpy(ptr, ACCOUNT_NAME_FIELD);
  ptr = stpcpy(ptr, aname);
  ptr = stpcpy(ptr, "\",");
  ptr = stpcpy(ptr, MDT_FIELD);
  ptr = stpcpy(ptr, mdt);
  ptr = stpcpy(ptr, "\",");
  ptr = stpcpy(ptr, MESSAGE_FIELD);
  ptr += base64(message, strlen(message), ptr);
  (void)stpcpy(ptr, "\"}");

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_field);
  curl_easy_setopt(curl, CURLOPT_URL, VZW_M2M_REST_API "/devices/nidd/message");
  curl_easy_default_opts(curl, &header_data, response_data, headers);

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
