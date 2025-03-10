/*
 * request_auth.h
 *
 * 
 */

#ifndef _request_auth_H_
#define _request_auth_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct request_auth_t request_auth_t;




typedef struct request_auth_t {
    char *password; // string
    char *username; // string

} request_auth_t;

request_auth_t *request_auth_create(
    char *password,
    char *username
);

void request_auth_free(request_auth_t *request_auth);

request_auth_t *request_auth_parseFromJSON(cJSON *request_authJSON);

cJSON *request_auth_convertToJSON(request_auth_t *request_auth);

#endif /* _request_auth_H_ */

