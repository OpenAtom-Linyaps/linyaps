#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "response_sign_in.h"



response_sign_in_t *response_sign_in_create(
    char *token
    ) {
    response_sign_in_t *response_sign_in_local_var = malloc(sizeof(response_sign_in_t));
    if (!response_sign_in_local_var) {
        return NULL;
    }
    response_sign_in_local_var->token = token;

    return response_sign_in_local_var;
}


void response_sign_in_free(response_sign_in_t *response_sign_in) {
    if(NULL == response_sign_in){
        return ;
    }
    listEntry_t *listEntry;
    if (response_sign_in->token) {
        free(response_sign_in->token);
        response_sign_in->token = NULL;
    }
    free(response_sign_in);
}

cJSON *response_sign_in_convertToJSON(response_sign_in_t *response_sign_in) {
    cJSON *item = cJSON_CreateObject();

    // response_sign_in->token
    if(response_sign_in->token) {
    if(cJSON_AddStringToObject(item, "token", response_sign_in->token) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

response_sign_in_t *response_sign_in_parseFromJSON(cJSON *response_sign_inJSON){

    response_sign_in_t *response_sign_in_local_var = NULL;

    // response_sign_in->token
    cJSON *token = cJSON_GetObjectItemCaseSensitive(response_sign_inJSON, "token");
    if (token) { 
    if(!cJSON_IsString(token) && !cJSON_IsNull(token))
    {
    goto end; //String
    }
    }


    response_sign_in_local_var = response_sign_in_create (
        token && !cJSON_IsNull(token) ? strdup(token->valuestring) : NULL
        );

    return response_sign_in_local_var;
end:
    return NULL;

}
