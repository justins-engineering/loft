/** @file credentials.h
 *  @brief Macro and function defines for the vzw thingspace credentials.
 */

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

/** @fn int get_vzw_auth_token(const char *username, const char *password, const
 * char *vzw_auth_keys, char *vzw_auth_token)
 *  @brief Retrieves a Thingspace auth token through a POST request.
 */
int get_vzw_auth_token(const char *vzw_auth_keys, char *vzw_auth_token);

/** @fn int get_vzw_m2m_token(const char *username, const char *password, const
 * char *vzw_auth_token, char *vzw_m2m_token)
 *  @brief Retrieves a Thingspace M2M token through a POST request.
 */
int get_vzw_m2m_token(
    const char *username, const char *password, const char *vzw_auth_token, char *vzw_m2m_token
);
#endif  // CREDENTIALS_H
