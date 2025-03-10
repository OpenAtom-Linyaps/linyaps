#ifndef schema_new_upload_task_req_TEST
#define schema_new_upload_task_req_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define schema_new_upload_task_req_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/schema_new_upload_task_req.h"
schema_new_upload_task_req_t* instantiate_schema_new_upload_task_req(int include_optional);



schema_new_upload_task_req_t* instantiate_schema_new_upload_task_req(int include_optional) {
  schema_new_upload_task_req_t* schema_new_upload_task_req = NULL;
  if (include_optional) {
    schema_new_upload_task_req = schema_new_upload_task_req_create(
      "0",
      "0"
    );
  } else {
    schema_new_upload_task_req = schema_new_upload_task_req_create(
      "0",
      "0"
    );
  }

  return schema_new_upload_task_req;
}


#ifdef schema_new_upload_task_req_MAIN

void test_schema_new_upload_task_req(int include_optional) {
    schema_new_upload_task_req_t* schema_new_upload_task_req_1 = instantiate_schema_new_upload_task_req(include_optional);

	cJSON* jsonschema_new_upload_task_req_1 = schema_new_upload_task_req_convertToJSON(schema_new_upload_task_req_1);
	printf("schema_new_upload_task_req :\n%s\n", cJSON_Print(jsonschema_new_upload_task_req_1));
	schema_new_upload_task_req_t* schema_new_upload_task_req_2 = schema_new_upload_task_req_parseFromJSON(jsonschema_new_upload_task_req_1);
	cJSON* jsonschema_new_upload_task_req_2 = schema_new_upload_task_req_convertToJSON(schema_new_upload_task_req_2);
	printf("repeating schema_new_upload_task_req:\n%s\n", cJSON_Print(jsonschema_new_upload_task_req_2));
}

int main() {
  test_schema_new_upload_task_req(1);
  test_schema_new_upload_task_req(0);

  printf("Hello world \n");
  return 0;
}

#endif // schema_new_upload_task_req_MAIN
#endif // schema_new_upload_task_req_TEST
