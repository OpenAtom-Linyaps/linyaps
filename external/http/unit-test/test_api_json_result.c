#ifndef api_json_result_TEST
#define api_json_result_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define api_json_result_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/api_json_result.h"
api_json_result_t* instantiate_api_json_result(int include_optional);



api_json_result_t* instantiate_api_json_result(int include_optional) {
  api_json_result_t* api_json_result = NULL;
  if (include_optional) {
    api_json_result = api_json_result_create(
      56,
      0,
      "0",
      "0"
    );
  } else {
    api_json_result = api_json_result_create(
      56,
      0,
      "0",
      "0"
    );
  }

  return api_json_result;
}


#ifdef api_json_result_MAIN

void test_api_json_result(int include_optional) {
    api_json_result_t* api_json_result_1 = instantiate_api_json_result(include_optional);

	cJSON* jsonapi_json_result_1 = api_json_result_convertToJSON(api_json_result_1);
	printf("api_json_result :\n%s\n", cJSON_Print(jsonapi_json_result_1));
	api_json_result_t* api_json_result_2 = api_json_result_parseFromJSON(jsonapi_json_result_1);
	cJSON* jsonapi_json_result_2 = api_json_result_convertToJSON(api_json_result_2);
	printf("repeating api_json_result:\n%s\n", cJSON_Print(jsonapi_json_result_2));
}

int main() {
  test_api_json_result(1);
  test_api_json_result(0);

  printf("Hello world \n");
  return 0;
}

#endif // api_json_result_MAIN
#endif // api_json_result_TEST
