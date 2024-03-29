#pragma once

#include <context_reflection_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Contains API for context reflection system -- system for generating reflection data.
///
/// \par Definition
/// \parblock
/// Reflection data might come from different sources -- generated from code and saved statically, loaded from dynamic
/// plugins or parsed from scripted modules -- and also it might need to be processed and extended, for example with
/// convenience data types for events or with some autogenerated types. Reflection system goal is to unite reflection
/// collection and processing logic.
/// \endparblock
///
/// \par Generation algorithm
/// \parblock
/// Reflection registry is generated on reflection system init and when `kan_reflection_system_invalidate` is called.
/// Reflection generation consists of the following steps:
///
/// - Statically generated data is added to new registry. This data must be registered through build system, see
///   `reflection_statics.cmake` for more info.
/// - On-populate connections are called. To connect other system to on-populate routine use
///   `kan_reflection_system_connect_on_populate`. These connections can freely add everything to given registry.
/// - Iterative generation is started. Generation iteration connections (
///   `kan_reflection_system_disconnect_on_generation_iterate`) are called. They receive information about changes
///   from the last iteration and submit new changes through received `kan_reflection_system_generation_iterator_t`.
///   Keep in mind that on first iteration there is no changes, only filled registry. Iterations are executed one
///   after another until no changes are made.
/// - Now registry is considered generated and on-generated connections (`kan_reflection_system_connect_on_generated`)
///   are called.
///
/// This algorithm makes it possible to create complex interconnected generation logic without explicitly setting
/// dependencies between different generators. It might not be good for performance, but makes generation a lot easier.
/// \endparblock
///
/// \par Changing reflection data
/// \parblock
/// It is well-known that reflection registry does not technically support changes in added entries. But there is a
/// catch: if iterative generator has saved mutable pointer to generated entry, then it is technically safe to add
/// more structure fields, enum values or function arguments and also rearrange them (without changing names). It is
/// safe only during iterative generation and is not safe in other situations. This behaviour might be improved later.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Reflection system functions are not thread safe. `kan_reflection_system_generation_iterator_t` is not thread safe
/// too, but separate instance is provided to every generation iteration functor so functor can safely operate on it
/// without caring about thread safety.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief System name for requirements and queries.
#define KAN_CONTEXT_REFLECTION_SYSTEM_NAME "reflection_system_t"

typedef void (*kan_context_reflection_populate_t) (kan_context_system_handle_t other_system,
                                                   kan_reflection_registry_t registry);

typedef void (*kan_context_reflection_generated_t) (kan_context_system_handle_t other_system,
                                                    kan_reflection_registry_t registry,
                                                    kan_reflection_migration_seed_t migration_seed,
                                                    kan_reflection_struct_migrator_t migrator);

typedef uint64_t kan_reflection_system_generation_iterator_t;

typedef void (*kan_context_reflection_generation_iterate_t) (kan_context_system_handle_t other_system,
                                                             kan_reflection_registry_t registry,
                                                             kan_reflection_system_generation_iterator_t iterator,
                                                             uint64_t iteration_index);

/// \brief Connect other system as on-populate delegate.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_connect_on_populate (
    kan_context_system_handle_t reflection_system,
    kan_context_system_handle_t other_system,
    kan_context_reflection_populate_t functor);

/// \brief Disconnect other system from on-populate delegates.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_disconnect_on_populate (
    kan_context_system_handle_t reflection_system, kan_context_system_handle_t other_system);

/// \brief Connect other system as on-generation-iterate delegate.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_connect_on_generation_iterate (
    kan_context_system_handle_t reflection_system,
    kan_context_system_handle_t other_system,
    kan_context_reflection_generation_iterate_t functor);

/// \brief Disconnect other system from on-generation-iterate delegates.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_disconnect_on_generation_iterate (
    kan_context_system_handle_t reflection_system, kan_context_system_handle_t other_system);

/// \brief Connect other system as on-generated delegate.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_connect_on_generated (
    kan_context_system_handle_t reflection_system,
    kan_context_system_handle_t other_system,
    kan_context_reflection_generated_t functor);

/// \brief Disconnect other system from on-generated delegates.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_disconnect_on_generated (
    kan_context_system_handle_t reflection_system, kan_context_system_handle_t other_system);

/// \brief Returns latest reflection registry if it exists.
CONTEXT_REFLECTION_SYSTEM_API kan_reflection_registry_t
kan_reflection_system_get_registry (kan_context_system_handle_t reflection_system);

/// \brief Invalidate current reflection and triggers generation of new reflection.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_invalidate (kan_context_system_handle_t reflection_system);

/// \brief Return name of the next added enum or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_added_enum (kan_reflection_system_generation_iterator_t iterator);

/// \brief Return name of the next added struct or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_added_struct (kan_reflection_system_generation_iterator_t iterator);

/// \brief Return name of the next added function or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_added_function (kan_reflection_system_generation_iterator_t iterator);

/// \brief Return name of the next changed enum or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_changed_enum (kan_reflection_system_generation_iterator_t iterator);

/// \brief Return name of the next changed struct or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_changed_struct (kan_reflection_system_generation_iterator_t iterator);

/// \brief Return name of the next changed function or NULL if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API kan_interned_string_t
kan_reflection_system_generation_iterator_next_changed_function (kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_enum_meta_t
{
    kan_interned_string_t enum_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added enum meta or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_enum_meta_t
kan_reflection_system_generation_iterator_next_added_enum_meta (kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_enum_value_meta_t
{
    kan_interned_string_t enum_name;
    kan_interned_string_t enum_value_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added enum value meta or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_enum_value_meta_t
kan_reflection_system_generation_iterator_next_added_enum_value_meta (
    kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_struct_meta_t
{
    kan_interned_string_t struct_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added struct meta or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_struct_meta_t
kan_reflection_system_generation_iterator_next_added_struct_meta (kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_struct_field_meta_t
{
    kan_interned_string_t struct_name;
    kan_interned_string_t struct_field_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added struct field meta or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_struct_field_meta_t
kan_reflection_system_generation_iterator_next_added_struct_field_meta (
    kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_function_meta_t
{
    kan_interned_string_t function_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added function or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_function_meta_t
kan_reflection_system_generation_iterator_next_added_function_meta (
    kan_reflection_system_generation_iterator_t iterator);

struct kan_reflection_system_added_function_argument_meta_t
{
    kan_interned_string_t function_name;
    kan_interned_string_t function_argument_name;
    kan_interned_string_t meta_type_name;
};

/// \brief Return info about next added function argument meta or structure with NULLs if all events are processed.
CONTEXT_REFLECTION_SYSTEM_API struct kan_reflection_system_added_function_argument_meta_t
kan_reflection_system_generation_iterator_next_added_function_argument_meta (
    kan_reflection_system_generation_iterator_t iterator);

/// \brief Safely add enum in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_enum (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_enum_t *data);

/// \brief Safely add struct in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_struct (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_struct_t *data);

/// \brief Safely add function in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_function (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_function_t *data);

/// \brief Safely mark enum as changed in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_enum_changed (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_enum_t *data);

/// \brief Safely mark struct as changed in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_struct_changed (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_struct_t *data);

/// \brief Safely mark function as changed in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_function_changed (
    kan_reflection_system_generation_iterator_t iterator, const struct kan_reflection_function_t *data);

/// \brief Safely add enum meta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_enum_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t enum_name,
    kan_interned_string_t meta_type_name,
    void *meta);

/// \brief Safely add enum value eta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_enum_value_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t enum_name,
    kan_interned_string_t value_name,
    kan_interned_string_t meta_type_name,
    void *meta);

/// \brief Safely add struct meta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_struct_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t struct_name,
    kan_interned_string_t meta_type_name,
    void *meta);

/// \brief Safely add struct field meta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_struct_field_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t struct_name,
    kan_interned_string_t field_name,
    kan_interned_string_t meta_type_name,
    void *meta);

/// \brief Safely add function meta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_function_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t function_name,
    kan_interned_string_t meta_type_name,
    void *meta);

/// \brief Safely add function argument meta in current generation iteration.
CONTEXT_REFLECTION_SYSTEM_API void kan_reflection_system_generation_iterator_add_function_argument_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t function_name,
    kan_interned_string_t argument_name,
    kan_interned_string_t meta_type_name,
    void *meta);

KAN_C_HEADER_END
