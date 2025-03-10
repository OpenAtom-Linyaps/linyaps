#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "api_upload_task_file_resp.h"



api_upload_task_file_resp_t *api_upload_task_file_resp_create(
    int code,
    response_upload_task_resp_t *data,
    char *msg,
    char *trace_id
    ) {
    api_upload_task_file_resp_t *api_upload_task_file_resp_local_var = malloc(sizeof(api_upload_task_file_resp_t));
    if (!api_upload_task_file_resp_local_var) {
        return NULL;
    }
    api_upload_task_file_resp_local_var->code = code;
    api_upload_task_file_resp_local_var->data = data;
    api_upload_task_file_resp_local_var->msg = msg;
    api_upload_task_file_resp_local_var->trace_id = trace_id;

    return api_upload_task_file_resp_local_var;
}


void api_upload_task_file_resp_free(api_upload_task_file_resp_t *api_upload_task_file_resp) {
    if(NULL == api_upload_task_file_resp){
        return ;
    }
    listEntry_t *listEntry;
    if (api_upload_task_file_resp->data) {
        response_upload_task_resp_free(api_upload_task_file_resp->data);
        api_upload_task_file_resp->data = NULL;
    }
    if (api_upload_task_file_resp->msg) {
        free(api_upload_task_file_resp->msg);
        api_upload_task_file_resp->msg = NULL;
    }
    if (api_upload_task_file_resp->trace_id) {
        free(api_upload_task_file_resp->trace_id);
        api_upload_task_file_resp->trace_id = NULL;
    }
    free(api_upload_task_file_resp);
}

cJSON *api_upload_task_file_resp_convertToJSON(api_upload_task_file_resp_t *api_upload_task_file_resp) {
    cJSON *item = cJSON_CreateObject();

    // api_upload_task_file_resp->code
    if(api_upload_task_file_resp->code) {
    if(cJSON_AddNumberToObject(item, "code", api_upload_task_file_resp->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // api_upload_task_file_resp->data
    if(api_upload_task_file_resp->data) {
    cJSON *data_local_JSON = response_upload_task_resp_convertToJSON(api_upload_task_file_resp->data);
    if(data_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // api_upload_task_file_resp->msg
    if(api_upload_task_file_resp->msg) {
    if(cJSON_AddStringToObject(item, "msg", api_upload_task_file_resp->msg) == NULL) {
    goto fail; //String
    }
    }


    // api_upload_task_file_resp->trace_id
    if(api_upload_task_file_resp->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", api_upload_task_file_resp->trace_id) == NULL) {
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

api_upload_task_file_resp_t *api_upload_task_file_resp_parseFromJSON(cJSON *api_upload_task_file_respJSON){

    api_upload_task_file_resp_t *api_upload_task_file_resp_local_var = NULL;

    // define the local variable for api_upload_task_file_resp->data
    response_upload_task_resp_t *data_local_nonprim = NULL;

    // api_upload_task_file_resp->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(api_upload_task_file_respJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // api_upload_task_file_resp->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(api_upload_task_file_respJSON, "data");
    if (data) { 
    data_local_nonprim = response_upload_task_resp_parseFromJSON(data); //nonprimitive
    }

    // api_upload_task_file_resp->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(api_upload_task_file_respJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // api_upload_task_file_resp->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(api_upload_task_file_respJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    api_upload_task_file_resp_local_var = api_upload_task_file_resp_create (
        code ? code->valuedouble : 0,
        data ? data_local_nonprim : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return api_upload_task_file_resp_local_var;
end:
    if (data_local_nonprim) {
        response_upload_task_resp_free(data_local_nonprim);
        data_local_nonprim = NULL;
    }
    return NULL;

}
