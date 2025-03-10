/*
 * response_sign_in.h
 *
 * 
 */

#ifndef _response_sign_in_H_
#define _response_sign_in_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct response_sign_in_t response_sign_in_t;




typedef struct response_sign_in_t {
    char *token; // string

} response_sign_in_t;

response_sign_in_t *response_sign_in_create(
    char *token
);

void response_sign_in_free(response_sign_in_t *response_sign_in);

response_sign_in_t *response_sign_in_parseFromJSON(cJSON *response_sign_inJSON);

cJSON *response_sign_in_convertToJSON(response_sign_in_t *response_sign_in);

#endif /* _response_sign_in_H_ */

