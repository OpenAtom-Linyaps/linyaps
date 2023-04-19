# Unit Testing

Linglong use gtest for unit test, just pull test code and testdata in test/ directory of project root.

Seem some test is hard to run in ci envionment, we disable it by default.

Here are some envionment variables to control if test run, all of them are empty by default.

|name| vaule|
|-|-|
|LINGLONG_TEST_ALL|If set, will run all unit testing. |