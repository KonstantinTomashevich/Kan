#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/render_pipeline_language/compiler.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_rpl_emitter_t;

RENDER_PIPELINE_LANGUAGE_API kan_rpl_emitter_t kan_rpl_emitter_create (kan_interned_string_t log_name,
                                                                       enum kan_rpl_pipeline_type_t pipeline_type,
                                                                       struct kan_rpl_intermediate_t *intermediate);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_set_flag_option (kan_rpl_emitter_t emitter,
                                                                         kan_interned_string_t name,
                                                                         kan_bool_t value);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_set_count_option (kan_rpl_emitter_t emitter,
                                                                          kan_interned_string_t name,
                                                                          uint64_t value);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_validate (kan_rpl_emitter_t emitter);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_emit_meta (kan_rpl_emitter_t emitter,
                                                                   struct kan_rpl_meta_t *meta_output);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_emit_code_spirv (kan_rpl_emitter_t emitter,
                                                                         kan_interned_string_t entry_function_name,
                                                                         enum kan_rpl_pipeline_stage_t stage,
                                                                         struct kan_dynamic_array_t *code_output);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_emitter_destroy (kan_rpl_emitter_t emitter);

KAN_C_HEADER_END
