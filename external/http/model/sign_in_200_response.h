/*
 * sign_in_200_response.h
 *
 * 
 */

#ifndef _sign_in_200_response_H_
#define _sign_in_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct sign_in_200_response_t sign_in_200_response_t;

#include "response_sign_in.h"



typedef struct sign_in_200_response_t {
    int code; //numeric
    struct response_sign_in_t *data; //model
    char *msg; // string
    char *trace_id; // string

} sign_in_200_response_t;

sign_in_200_response_t *sign_in_200_response_create(
    int code,
    response_sign_in_t *data,
    char *msg,
    char *trace_id
);

void sign_in_200_response_free(sign_in_200_response_t *sign_in_200_response);

sign_in_200_response_t *sign_in_200_response_parseFromJSON(cJSON *sign_in_200_responseJSON);

cJSON *sign_in_200_response_convertToJSON(sign_in_200_response_t *sign_in_200_response);

#endif /* _sign_in_200_response_H_ */

