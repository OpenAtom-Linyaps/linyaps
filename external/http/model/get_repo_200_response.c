#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "get_repo_200_response.h"



get_repo_200_response_t *get_repo_200_response_create(
    int code,
    schema_repo_info_t *data,
    char *msg,
    char *trace_id
    ) {
    get_repo_200_response_t *get_repo_200_response_local_var = malloc(sizeof(get_repo_200_response_t));
    if (!get_repo_200_response_local_var) {
        return NULL;
    }
    get_repo_200_response_local_var->code = code;
    get_repo_200_response_local_var->data = data;
    get_repo_200_response_local_var->msg = msg;
    get_repo_200_response_local_var->trace_id = trace_id;

    return get_repo_200_response_local_var;
}


void get_repo_200_response_free(get_repo_200_response_t *get_repo_200_response) {
    if(NULL == get_repo_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (get_repo_200_response->data) {
        schema_repo_info_free(get_repo_200_response->data);
        get_repo_200_response->data = NULL;
    }
    if (get_repo_200_response->msg) {
        free(get_repo_200_response->msg);
        get_repo_200_response->msg = NULL;
    }
    if (get_repo_200_response->trace_id) {
        free(get_repo_200_response->trace_id);
        get_repo_200_response->trace_id = NULL;
    }
    free(get_repo_200_response);
}

cJSON *get_repo_200_response_convertToJSON(get_repo_200_response_t *get_repo_200_response) {
    cJSON *item = cJSON_CreateObject();

    // get_repo_200_response->code
    if(get_repo_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", get_repo_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // get_repo_200_response->data
    if(get_repo_200_response->data) {
    cJSON *data_local_JSON = schema_repo_info_convertToJSON(get_repo_200_response->data);
    if(data_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // get_repo_200_response->msg
    if(get_repo_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", get_repo_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // get_repo_200_response->trace_id
    if(get_repo_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", get_repo_200_response->trace_id) == NULL) {
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

get_repo_200_response_t *get_repo_200_response_parseFromJSON(cJSON *get_repo_200_responseJSON){

    get_repo_200_response_t *get_repo_200_response_local_var = NULL;

    // define the local variable for get_repo_200_response->data
    schema_repo_info_t *data_local_nonprim = NULL;

    // get_repo_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(get_repo_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // get_repo_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(get_repo_200_responseJSON, "data");
    if (data) { 
    data_local_nonprim = schema_repo_info_parseFromJSON(data); //nonprimitive
    }

    // get_repo_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(get_repo_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // get_repo_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(get_repo_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    get_repo_200_response_local_var = get_repo_200_response_create (
        code ? code->valuedouble : 0,
        data ? data_local_nonprim : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return get_repo_200_response_local_var;
end:
    if (data_local_nonprim) {
        schema_repo_info_free(data_local_nonprim);
        data_local_nonprim = NULL;
    }
    return NULL;

}
