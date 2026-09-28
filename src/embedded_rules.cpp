#include "snip/embedded_rules.hpp"

namespace snip {

static const std::string_view DOCKER_PS_TOML = R"toml(
schema_version = "1.0"
name = "docker-ps"
description = "Prunes docker container listings to high-signal columns"
strategy = "columnar"

[match]
binary = "docker"
subcommands = ["ps", "container ls"]
exit_codes = [0]

[columnar]
header_row = 0
keep_columns = ["CONTAINER ID", "IMAGE", "STATUS", "PORTS"]
drop_empty_columns = true
max_rows = 50
truncation_indicator = "[...+containers omitted...]"
)toml";

static const std::string_view KUBECTL_GET_TOML = R"toml(
schema_version = "1.0"
name = "kubectl-get"
description = "Prunes kubectl resources to core status columns"
strategy = "columnar"

[match]
binary = "kubectl"
subcommands = ["get"]
exit_codes = [0]

[columnar]
header_row = 0
keep_columns = ["NAME", "READY", "STATUS", "RESTARTS", "AGE"]
drop_empty_columns = true
max_rows = 50
truncation_indicator = "[...+resources omitted...]"
)toml";

const std::vector<EmbeddedRuleEntry>& EmbeddedRules::get_all() {
    static const std::vector<EmbeddedRuleEntry> rules = {
        { "docker-ps", DOCKER_PS_TOML },
        { "kubectl-get", KUBECTL_GET_TOML }
    };
    return rules;
}

} // namespace snip
