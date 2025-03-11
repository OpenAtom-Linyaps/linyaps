#ifndef apiv2_json_error_TEST
#define apiv2_json_error_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define apiv2_json_error_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/apiv2_json_error.h"
apiv2_json_error_t* instantiate_apiv2_json_error(int include_optional);



apiv2_json_error_t* instantiate_apiv2_json_error(int include_optional) {
  apiv2_json_error_t* apiv2_json_error = NULL;
  if (include_optional) {
    apiv2_json_error = apiv2_json_error_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  } else {
    apiv2_json_error = apiv2_json_error_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  }

  return apiv2_json_error;
}


#ifdef apiv2_json_error_MAIN

void test_apiv2_json_error(int include_optional) {
    apiv2_json_error_t* apiv2_json_error_1 = instantiate_apiv2_json_error(include_optional);

	cJSON* jsonapiv2_json_error_1 = apiv2_json_error_convertToJSON(apiv2_json_error_1);
	printf("apiv2_json_error :\n%s\n", cJSON_Print(jsonapiv2_json_error_1));
	apiv2_json_error_t* apiv2_json_error_2 = apiv2_json_error_parseFromJSON(jsonapiv2_json_error_1);
	cJSON* jsonapiv2_json_error_2 = apiv2_json_error_convertToJSON(apiv2_json_error_2);
	printf("repeating apiv2_json_error:\n%s\n", cJSON_Print(jsonapiv2_json_error_2));
}

int main() {
  test_apiv2_json_error(1);
  test_apiv2_json_error(0);

  printf("Hello world \n");
  return 0;
}

#endif // apiv2_json_error_MAIN
#endif // apiv2_json_error_TEST
