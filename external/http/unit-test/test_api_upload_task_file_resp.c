#ifndef api_upload_task_file_resp_TEST
#define api_upload_task_file_resp_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define api_upload_task_file_resp_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/api_upload_task_file_resp.h"
api_upload_task_file_resp_t* instantiate_api_upload_task_file_resp(int include_optional);

#include "test_response_upload_task_resp.c"


api_upload_task_file_resp_t* instantiate_api_upload_task_file_resp(int include_optional) {
  api_upload_task_file_resp_t* api_upload_task_file_resp = NULL;
  if (include_optional) {
    api_upload_task_file_resp = api_upload_task_file_resp_create(
      56,
       // false, not to have infinite recursion
      instantiate_response_upload_task_resp(0),
      "0",
      "0"
    );
  } else {
    api_upload_task_file_resp = api_upload_task_file_resp_create(
      56,
      NULL,
      "0",
      "0"
    );
  }

  return api_upload_task_file_resp;
}


#ifdef api_upload_task_file_resp_MAIN

void test_api_upload_task_file_resp(int include_optional) {
    api_upload_task_file_resp_t* api_upload_task_file_resp_1 = instantiate_api_upload_task_file_resp(include_optional);

	cJSON* jsonapi_upload_task_file_resp_1 = api_upload_task_file_resp_convertToJSON(api_upload_task_file_resp_1);
	printf("api_upload_task_file_resp :\n%s\n", cJSON_Print(jsonapi_upload_task_file_resp_1));
	api_upload_task_file_resp_t* api_upload_task_file_resp_2 = api_upload_task_file_resp_parseFromJSON(jsonapi_upload_task_file_resp_1);
	cJSON* jsonapi_upload_task_file_resp_2 = api_upload_task_file_resp_convertToJSON(api_upload_task_file_resp_2);
	printf("repeating api_upload_task_file_resp:\n%s\n", cJSON_Print(jsonapi_upload_task_file_resp_2));
}

int main() {
  test_api_upload_task_file_resp(1);
  test_api_upload_task_file_resp(0);

  printf("Hello world \n");
  return 0;
}

#endif // api_upload_task_file_resp_MAIN
#endif // api_upload_task_file_resp_TEST
