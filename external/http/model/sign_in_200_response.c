#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sign_in_200_response.h"



sign_in_200_response_t *sign_in_200_response_create(
    int code,
    response_sign_in_t *data,
    char *msg,
    char *trace_id
    ) {
    sign_in_200_response_t *sign_in_200_response_local_var = malloc(sizeof(sign_in_200_response_t));
    if (!sign_in_200_response_local_var) {
        return NULL;
    }
    sign_in_200_response_local_var->code = code;
    sign_in_200_response_local_var->data = data;
    sign_in_200_response_local_var->msg = msg;
    sign_in_200_response_local_var->trace_id = trace_id;

    return sign_in_200_response_local_var;
}


void sign_in_200_response_free(sign_in_200_response_t *sign_in_200_response) {
    if(NULL == sign_in_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (sign_in_200_response->data) {
        response_sign_in_free(sign_in_200_response->data);
        sign_in_200_response->data = NULL;
    }
    if (sign_in_200_response->msg) {
        free(sign_in_200_response->msg);
        sign_in_200_response->msg = NULL;
    }
    if (sign_in_200_response->trace_id) {
        free(sign_in_200_response->trace_id);
        sign_in_200_response->trace_id = NULL;
    }
    free(sign_in_200_response);
}

cJSON *sign_in_200_response_convertToJSON(sign_in_200_response_t *sign_in_200_response) {
    cJSON *item = cJSON_CreateObject();

    // sign_in_200_response->code
    if(sign_in_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", sign_in_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // sign_in_200_response->data
    if(sign_in_200_response->data) {
    cJSON *data_local_JSON = response_sign_in_convertToJSON(sign_in_200_response->data);
    if(data_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // sign_in_200_response->msg
    if(sign_in_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", sign_in_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // sign_in_200_response->trace_id
    if(sign_in_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", sign_in_200_response->trace_id) == NULL) {
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

sign_in_200_response_t *sign_in_200_response_parseFromJSON(cJSON *sign_in_200_responseJSON){

    sign_in_200_response_t *sign_in_200_response_local_var = NULL;

    // define the local variable for sign_in_200_response->data
    response_sign_in_t *data_local_nonprim = NULL;

    // sign_in_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(sign_in_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // sign_in_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(sign_in_200_responseJSON, "data");
    if (data) { 
    data_local_nonprim = response_sign_in_parseFromJSON(data); //nonprimitive
    }

    // sign_in_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(sign_in_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // sign_in_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(sign_in_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    sign_in_200_response_local_var = sign_in_200_response_create (
        code ? code->valuedouble : 0,
        data ? data_local_nonprim : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return sign_in_200_response_local_var;
end:
    if (data_local_nonprim) {
        response_sign_in_free(data_local_nonprim);
        data_local_nonprim = NULL;
    }
    return NULL;

}
