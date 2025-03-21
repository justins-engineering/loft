/** @file api_fields.h
 *  @brief Shared definitions and macros for the vzw thingspace APIs
 */

#ifndef API_FIELDS_H
#define API_FIELDS_H

#define CONTENT_TYPE_JSON "Content-Type:application/json"
#define ACCEPT_JSON "Accept:application/json"

#define VZW_M2M_REST_API "https://thingspace.verizon.com/api/m2m/v1"

#define OAUTH2_TOKEN_FIELD "Authorization:Basic "

/** Base64 encoding is deterministic, so we know the length ahead of time */
#define OAUTH2_TOKEN_FIELD_SIZE sizeof(OAUTH2_TOKEN_FIELD) + BASE64LEN(strlen(vzw_auth_keys))

#define ACCESS_TOKEN_FIELD "Authorization:Bearer "
#define ACCESS_TOKEN_FIELD_SIZE(t) sizeof(ACCESS_TOKEN_FIELD) + strlen(t)

#define SESSION_TOKEN_FIELD "VZ-M2M-Token:"
#define SESSION_TOKEN_FIELD_SIZE(t) sizeof(SESSION_TOKEN_FIELD) + strlen(t)

#define VZW_CALLBACKS_API VZW_M2M_REST_API "/callbacks/"
#define VZW_CALLBACKS_API_LEN(aname) sizeof(VZW_CALLBACKS_API) + strlen(aname)

#endif  // API_FIELDS_H
