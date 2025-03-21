/** @file vzw_connect.h
 *  @brief Macro and function defines for the vzw thingspace credentials.
 */

#ifndef VZW_CONNECT_H
#define VZW_CONNECT_H

#include <definitions.h>

int get_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data
);

int set_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data,
    char *service_name, char *url
);

int delete_registered_callback_listeners(
    const char *account_name, char *vzw_auth_token, char *vzw_m2m_token, CharBuff *response_data,
    char *service_name
);

int send_nidd_data(
    const char *aname, char *vzw_auth_token, char *vzw_m2m_token, char *mdn, char *mdt,
    char *message, CharBuff *response_data
);
#endif
