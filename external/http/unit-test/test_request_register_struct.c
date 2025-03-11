#ifndef request_register_struct_TEST
#define request_register_struct_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define request_register_struct_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/request_register_struct.h"
request_register_struct_t* instantiate_request_register_struct(int include_optional);



request_register_struct_t* instantiate_request_register_struct(int include_optional) {
  request_register_struct_t* request_register_struct = NULL;
  if (include_optional) {
    request_register_struct = request_register_struct_create(
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      56,
      "0",
      "0"
    );
  } else {
    request_register_struct = request_register_struct_create(
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      56,
      "0",
      "0"
    );
  }

  return request_register_struct;
}


#ifdef request_register_struct_MAIN

void test_request_register_struct(int include_optional) {
    request_register_struct_t* request_register_struct_1 = instantiate_request_register_struct(include_optional);

	cJSON* jsonrequest_register_struct_1 = request_register_struct_convertToJSON(request_register_struct_1);
	printf("request_register_struct :\n%s\n", cJSON_Print(jsonrequest_register_struct_1));
	request_register_struct_t* request_register_struct_2 = request_register_struct_parseFromJSON(jsonrequest_register_struct_1);
	cJSON* jsonrequest_register_struct_2 = request_register_struct_convertToJSON(request_register_struct_2);
	printf("repeating request_register_struct:\n%s\n", cJSON_Print(jsonrequest_register_struct_2));
}

int main() {
  test_request_register_struct(1);
  test_request_register_struct(0);

  printf("Hello world \n");
  return 0;
}

#endif // request_register_struct_MAIN
#endif // request_register_struct_TEST
