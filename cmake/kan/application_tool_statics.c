#include <stdint.h>

char *kan_application_framework_tool_plugins_directory = ${STATICS_PLUGINS_DIRECTORY_PATH};
char *kan_application_framework_tool_plugins[] = {
${STATICS_PLUGINS}
};

uint64_t kan_application_framework_tool_plugins_count =
    sizeof (kan_application_framework_tool_plugins) / sizeof (char *);
