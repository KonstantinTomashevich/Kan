#include <kan/context/context.h>

#if defined(_WIN32)
#    define IMPORT_THIS __declspec (dllimport)
#    define EXPORT_THIS __declspec (dllexport)
#else
#    define IMPORT_THIS
#    define EXPORT_THIS
#endif

${SYSTEM_APIS_DECLARATIONS};

EXPORT_THIS uint64_t KAN_CONTEXT_SYSTEM_COUNT_NAME = ${SYSTEMS_COUNT}u;
EXPORT_THIS struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[] = {
    ${SYSTEM_APIS_LIST},
};
