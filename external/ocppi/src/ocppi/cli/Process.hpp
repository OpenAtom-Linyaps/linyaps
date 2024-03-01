#pragma once

#include <string>
#include <vector>

int runProcess(const std::string &binaryPath,
               const std::vector<std::string> &args, std::string &output);

int runProcess(const std::string &binaryPath,
               const std::vector<std::string> &args);
