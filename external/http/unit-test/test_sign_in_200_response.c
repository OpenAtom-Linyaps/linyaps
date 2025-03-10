#ifndef sign_in_200_response_TEST
#define sign_in_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define sign_in_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/sign_in_200_response.h"
sign_in_200_response_t* instantiate_sign_in_200_response(int include_optional);

#include "test_response_sign_in.c"


sign_in_200_response_t* instantiate_sign_in_200_response(int include_optional) {
  sign_in_200_response_t* sign_in_200_response = NULL;
  if (include_optional) {
    sign_in_200_response = sign_in_200_response_create(
      56,
       // false, not to have infinite recursion
      instantiate_response_sign_in(0),
      "0",
      "0"
    );
  } else {
    sign_in_200_response = sign_in_200_response_create(
      56,
      NULL,
      "0",
      "0"
    );
  }

  return sign_in_200_response;
}


#ifdef sign_in_200_response_MAIN

void test_sign_in_200_response(int include_optional) {
    sign_in_200_response_t* sign_in_200_response_1 = instantiate_sign_in_200_response(include_optional);

	cJSON* jsonsign_in_200_response_1 = sign_in_200_response_convertToJSON(sign_in_200_response_1);
	printf("sign_in_200_response :\n%s\n", cJSON_Print(jsonsign_in_200_response_1));
	sign_in_200_response_t* sign_in_200_response_2 = sign_in_200_response_parseFromJSON(jsonsign_in_200_response_1);
	cJSON* jsonsign_in_200_response_2 = sign_in_200_response_convertToJSON(sign_in_200_response_2);
	printf("repeating sign_in_200_response:\n%s\n", cJSON_Print(jsonsign_in_200_response_2));
}

int main() {
  test_sign_in_200_response(1);
  test_sign_in_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // sign_in_200_response_MAIN
#endif // sign_in_200_response_TEST
