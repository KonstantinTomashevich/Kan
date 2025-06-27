#include <kan/context/context.h>

#if defined(_MSC_VER)
#    define IMPORT_THIS __declspec (dllimport)
#    define EXPORT_THIS __declspec (dllexport)
#else
#    define IMPORT_THIS
#    define EXPORT_THIS
#endif

${SYSTEM_APIS_DECLARATIONS};

EXPORT_THIS kan_instance_size_t KAN_CONTEXT_SYSTEM_COUNT_NAME = ${SYSTEMS_COUNT}u;
EXPORT_THIS struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[${SYSTEMS_COUNT}u];

EXPORT_THIS void KAN_CONTEXT_SYSTEM_ARRAY_INITIALIZER_NAME (void)
{
    // Although this initializers look compile time, they're actually link
    // time and therefore some compilers require initializer functions.
    ${SYSTEM_APIS_SETTERS};
}
