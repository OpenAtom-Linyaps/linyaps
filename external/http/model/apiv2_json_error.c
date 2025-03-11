#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "apiv2_json_error.h"



apiv2_json_error_t *apiv2_json_error_create(
    int code,
    list_t* fields,
    char *msg,
    char *trace_id
    ) {
    apiv2_json_error_t *apiv2_json_error_local_var = malloc(sizeof(apiv2_json_error_t));
    if (!apiv2_json_error_local_var) {
        return NULL;
    }
    apiv2_json_error_local_var->code = code;
    apiv2_json_error_local_var->fields = fields;
    apiv2_json_error_local_var->msg = msg;
    apiv2_json_error_local_var->trace_id = trace_id;

    return apiv2_json_error_local_var;
}


void apiv2_json_error_free(apiv2_json_error_t *apiv2_json_error) {
    if(NULL == apiv2_json_error){
        return ;
    }
    listEntry_t *listEntry;
    if (apiv2_json_error->fields) {
        list_ForEach(listEntry, apiv2_json_error->fields) {
            keyValuePair_t *localKeyValue = (keyValuePair_t*) listEntry->data;
            free (localKeyValue->key);
            free (localKeyValue->value);
            keyValuePair_free(localKeyValue);
        }
        list_freeList(apiv2_json_error->fields);
        apiv2_json_error->fields = NULL;
    }
    if (apiv2_json_error->msg) {
        free(apiv2_json_error->msg);
        apiv2_json_error->msg = NULL;
    }
    if (apiv2_json_error->trace_id) {
        free(apiv2_json_error->trace_id);
        apiv2_json_error->trace_id = NULL;
    }
    free(apiv2_json_error);
}

cJSON *apiv2_json_error_convertToJSON(apiv2_json_error_t *apiv2_json_error) {
    cJSON *item = cJSON_CreateObject();

    // apiv2_json_error->code
    if(apiv2_json_error->code) {
    if(cJSON_AddNumberToObject(item, "code", apiv2_json_error->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // apiv2_json_error->fields
    if(apiv2_json_error->fields) {
    cJSON *fields = cJSON_AddObjectToObject(item, "fields");
    if(fields == NULL) {
        goto fail; //primitive map container
    }
    cJSON *localMapObject = fields;
    listEntry_t *fieldsListEntry;
    if (apiv2_json_error->fields) {
    list_ForEach(fieldsListEntry, apiv2_json_error->fields) {
        keyValuePair_t *localKeyValue = (keyValuePair_t*)fieldsListEntry->data;
        if(cJSON_AddStringToObject(localMapObject, localKeyValue->key, (char*)localKeyValue->value) == NULL)
        {
            goto fail;
        }
    }
    }
    }


    // apiv2_json_error->msg
    if(apiv2_json_error->msg) {
    if(cJSON_AddStringToObject(item, "msg", apiv2_json_error->msg) == NULL) {
    goto fail; //String
    }
    }


    // apiv2_json_error->trace_id
    if(apiv2_json_error->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", apiv2_json_error->trace_id) == NULL) {
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

apiv2_json_error_t *apiv2_json_error_parseFromJSON(cJSON *apiv2_json_errorJSON){

    apiv2_json_error_t *apiv2_json_error_local_var = NULL;

    // define the local map for apiv2_json_error->fields
    list_t *fieldsList = NULL;

    // apiv2_json_error->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(apiv2_json_errorJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // apiv2_json_error->fields
    cJSON *fields = cJSON_GetObjectItemCaseSensitive(apiv2_json_errorJSON, "fields");
    if (fields) { 
    cJSON *fields_local_map = NULL;
    if(!cJSON_IsObject(fields) && !cJSON_IsNull(fields))
    {
        goto end;//primitive map container
    }
    if(cJSON_IsObject(fields))
    {
        fieldsList = list_createList();
        keyValuePair_t *localMapKeyPair;
        cJSON_ArrayForEach(fields_local_map, fields)
        {
            cJSON *localMapObject = fields_local_map;
            if(!cJSON_IsString(localMapObject))
            {
                goto end;
            }
            localMapKeyPair = keyValuePair_create(strdup(localMapObject->string),strdup(localMapObject->valuestring));
            list_addElement(fieldsList , localMapKeyPair);
        }
    }
    }

    // apiv2_json_error->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(apiv2_json_errorJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // apiv2_json_error->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(apiv2_json_errorJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    apiv2_json_error_local_var = apiv2_json_error_create (
        code ? code->valuedouble : 0,
        fields ? fieldsList : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return apiv2_json_error_local_var;
end:
    if (fieldsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, fieldsList) {
            keyValuePair_t *localKeyValue = (keyValuePair_t*) listEntry->data;
            free(localKeyValue->key);
            localKeyValue->key = NULL;
            free(localKeyValue->value);
            localKeyValue->value = NULL;
            keyValuePair_free(localKeyValue);
            localKeyValue = NULL;
        }
        list_freeList(fieldsList);
        fieldsList = NULL;
    }
    return NULL;

}
