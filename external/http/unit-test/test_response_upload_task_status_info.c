#ifndef response_upload_task_status_info_TEST
#define response_upload_task_status_info_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define response_upload_task_status_info_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/response_upload_task_status_info.h"
response_upload_task_status_info_t* instantiate_response_upload_task_status_info(int include_optional);



response_upload_task_status_info_t* instantiate_response_upload_task_status_info(int include_optional) {
  response_upload_task_status_info_t* response_upload_task_status_info = NULL;
  if (include_optional) {
    response_upload_task_status_info = response_upload_task_status_info_create(
      "0"
    );
  } else {
    response_upload_task_status_info = response_upload_task_status_info_create(
      "0"
    );
  }

  return response_upload_task_status_info;
}


#ifdef response_upload_task_status_info_MAIN

void test_response_upload_task_status_info(int include_optional) {
    response_upload_task_status_info_t* response_upload_task_status_info_1 = instantiate_response_upload_task_status_info(include_optional);

	cJSON* jsonresponse_upload_task_status_info_1 = response_upload_task_status_info_convertToJSON(response_upload_task_status_info_1);
	printf("response_upload_task_status_info :\n%s\n", cJSON_Print(jsonresponse_upload_task_status_info_1));
	response_upload_task_status_info_t* response_upload_task_status_info_2 = response_upload_task_status_info_parseFromJSON(jsonresponse_upload_task_status_info_1);
	cJSON* jsonresponse_upload_task_status_info_2 = response_upload_task_status_info_convertToJSON(response_upload_task_status_info_2);
	printf("repeating response_upload_task_status_info:\n%s\n", cJSON_Print(jsonresponse_upload_task_status_info_2));
}

int main() {
  test_response_upload_task_status_info(1);
  test_response_upload_task_status_info(0);

  printf("Hello world \n");
  return 0;
}

#endif // response_upload_task_status_info_MAIN
#endif // response_upload_task_status_info_TEST
