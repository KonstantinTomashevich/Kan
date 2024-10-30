#include <kan/render_backend_tools/render_backend_tools.h>

kan_bool_t kan_render_backend_tools_emit_platform_code (kan_rpl_compiler_instance_t compiler_instance,
                                                        struct kan_dynamic_array_t *output,
                                                        kan_allocation_group_t output_allocation_group)
{
    return kan_rpl_compiler_instance_emit_spirv (compiler_instance, output, output_allocation_group);
}
