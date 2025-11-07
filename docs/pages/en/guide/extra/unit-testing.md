# Unit Testing

Linyaps utilizes GoogleTest (gtest) as its unit testing framework. Simply include your test code and test data in the `test/` directory at the project root.

Since certain tests are challenging to execute in CI environments, they are disabled by default.

The following environment variables control test execution. All are empty by default:

| Variable Name     | Value                           | Description                     |
| ----------------- | ------------------------------- | ------------------------------- |
| LINGLONG_TEST_ALL | If set, enables all unit tests | Executes complete unit test suite |
