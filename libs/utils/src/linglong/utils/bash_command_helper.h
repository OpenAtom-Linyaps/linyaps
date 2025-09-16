#pragma once

#include <vector>
#include <string>
#include "linglong/utils/bash_quote.h"

namespace linglong::utils {

class BashCommandHelper {
public:
    static std::vector<std::string> generateDefaultBashCommand() {
        return {
            "env", "-i",
            "bash", "--norc", "--noprofile", 
            "-c", "source /etc/profile; bash --norc"
        };
    }

    static std::vector<std::string> generateExecCommand(
        const std::vector<std::string>& originalArgs) {
        return {
            "/run/linglong/container-init",
            "env", "-i",
            "/bin/bash", "--noprofile", "--norc",
            "-c", generateEntrypointScript(originalArgs)
        };
    }

public:
    static std::string generateEntrypointScript(const std::vector<std::string>& args) {
        std::string script = "#!/bin/bash\n";
        script += "source /etc/profile\n";
        script += "exec ";
        for (const auto& arg : args) {
            script += quoteBashArg(arg) + " ";
        }
        return script;
    }
private:
};

} // namespace linglong::utils
