#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "api_json_result.h"



api_json_result_t *api_json_result_create(
    int code,
    object_t *data,
    char *msg,
    char *trace_id
    ) {
    api_json_result_t *api_json_result_local_var = malloc(sizeof(api_json_result_t));
    if (!api_json_result_local_var) {
        return NULL;
    }
    api_json_result_local_var->code = code;
    api_json_result_local_var->data = data;
    api_json_result_local_var->msg = msg;
    api_json_result_local_var->trace_id = trace_id;

    return api_json_result_local_var;
}


void api_json_result_free(api_json_result_t *api_json_result) {
    if(NULL == api_json_result){
        return ;
    }
    listEntry_t *listEntry;
    if (api_json_result->data) {
        object_free(api_json_result->data);
        api_json_result->data = NULL;
    }
    if (api_json_result->msg) {
        free(api_json_result->msg);
        api_json_result->msg = NULL;
    }
    if (api_json_result->trace_id) {
        free(api_json_result->trace_id);
        api_json_result->trace_id = NULL;
    }
    free(api_json_result);
}

cJSON *api_json_result_convertToJSON(api_json_result_t *api_json_result) {
    cJSON *item = cJSON_CreateObject();

    // api_json_result->code
    if(api_json_result->code) {
    if(cJSON_AddNumberToObject(item, "code", api_json_result->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // api_json_result->data
    if(api_json_result->data) {
    cJSON *data_object = object_convertToJSON(api_json_result->data);
    if(data_object == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_object);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // api_json_result->msg
    if(api_json_result->msg) {
    if(cJSON_AddStringToObject(item, "msg", api_json_result->msg) == NULL) {
    goto fail; //String
    }
    }


    // api_json_result->trace_id
    if(api_json_result->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", api_json_result->trace_id) == NULL) {
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

api_json_result_t *api_json_result_parseFromJSON(cJSON *api_json_resultJSON){

    api_json_result_t *api_json_result_local_var = NULL;

    // api_json_result->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(api_json_resultJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // api_json_result->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(api_json_resultJSON, "data");
    object_t *data_local_object = NULL;
    if (data) { 
    data_local_object = object_parseFromJSON(data); //object
    }

    // api_json_result->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(api_json_resultJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // api_json_result->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(api_json_resultJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    api_json_result_local_var = api_json_result_create (
        code ? code->valuedouble : 0,
        data ? data_local_object : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return api_json_result_local_var;
end:
    return NULL;

}
