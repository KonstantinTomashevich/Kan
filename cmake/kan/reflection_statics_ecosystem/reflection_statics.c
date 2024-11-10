#include <kan/reflection/generated_reflection.h>

#if defined(_WIN32)
#    define IMPORT_THIS __declspec (dllimport)
#    define EXPORT_THIS __declspec (dllexport)
#else
#    define IMPORT_THIS
#    define EXPORT_THIS
#endif

${REFLECTION_REGISTRARS_DECLARATIONS};

EXPORT_THIS void kan_reflection_system_register_statics (kan_reflection_registry_t registry)
{
    ${REFLECTION_REGISTRARS_CALLS};
}
