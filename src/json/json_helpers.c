/** @headerfile json_helpers.h */
#include "json_helpers.h"

#include <definitions.h>

/** Compares a string with a jsmn token value. */
int jsoneq(
    const char *const json_ptr, const jsmntok_t *tok, const char *const string
) {
  if (tok->type == JSMN_STRING &&
      (int)strlen(string) == tok->end - tok->start &&
      strncmp(json_ptr + tok->start, string, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

int eval_jsmn_return(int ret) {
  switch (ret) {
    case 0:
      PRINTERR("Parsed Empty JSON string.\n");
      return 1;
    case JSMN_ERROR_NOMEM:
      PRINTERR("Failed to parse JSON; Not enough tokens were provided.");
      return 1;
    case JSMN_ERROR_INVAL:
      PRINTERR("Failed to parse JSON; Invalid character inside JSON string.");
      return 1;
    case JSMN_ERROR_PART:
      PRINTERR(
          "Failed to parse JSON; The string is not a full JSON packet, more "
          "bytes expected."
      );
      return 1;
    default:
      // PRINTSUCCES("Tokens allocated: %d/%d\n", ret, 8);
      return 0;
  }
}
