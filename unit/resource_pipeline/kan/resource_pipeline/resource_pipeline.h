#pragma once

#include <resource_pipeline_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Provides common utilities for building resource pipeline and interacting with it.
///
/// \par Resource marking
/// \parblock
/// Resource types should be marked with `kan_resource_resource_type_meta_t` minimalistic meta to be visible
/// for reflection-based resource logic.
/// \endparblock
///
/// \par Reference scanning
/// \parblock
/// Resource pipeline makes it possible to register fields of structs that reference resources using
/// `kan_resource_reference_meta_t` meta. Reflection registry can be scanned for this reference data and output
/// will be stored in `kan_resource_reference_type_info_storage_t`, which allows to make reference detection
/// algorithm faster. Then, references can be extracted from every resource type using
/// `kan_resource_detect_references` into serializable (and therefore cacheable)
/// `kan_resource_detected_reference_container_t`.
/// \endparblock
///
/// \par Compilation
/// \parblock
/// Resources can be marked as compilable using `kan_resource_compilable_meta_t` meta, which contains
/// essential data for resource compilation. When resource is compilable, it means that it would be replaced by
/// other, more optimized and ready for use, resource of specified type during resource building process. When
/// resource isn't compilable, it is just converted to binary format (with strings interned if specified by user).
///
/// Compilation is a process of transforming data to a more optimized and ready to use format, that is done through
/// compile functor. Compilation can be done inside resource building tool or by application in runtime if needed.
/// It makes compilation API more complex as it needs to support step-based compilation: that means that it should be
/// able to be split to several frames unless compilation is guaranteed to be fast.
///
/// Also, compilation can be different for different target platforms, therefore compilation platform configuration
/// feature is provided. When compiling in runtime, current platform configuration is provided.
///
/// One of the important features of compilations is byproduct support. It means that compilation can produce relatively
/// small resources that never existed in resource source directories. These resources support merging: if several
/// compilation requests produced the same byproduct, only one instance will be present in compiled resources.
/// Also, byproducts can be used to build complex multi-step compilation routines. Every byproduct type must have
/// `kan_resource_byproduct_type_meta_t` meta on it.
///
/// See more information in `kan_resource_compile_state_t` docs.
/// \endparblock
///
/// \par Import
/// \parblock
/// Import rule related types are provided to describe how resources could be imported. `kan_resource_import_rule_t`
/// contains rule for importing data into resource directory and is expected to be stored inside resource directory as
/// well. Import rule can reference one or more inputs through source path. It contains one configuration patch that
/// can be used to fetch the right import functor through `kan_resource_import_configuration_type_meta_t`. Also,
/// import rule stores last import data that can be used to check whether source files have changed and also to manage
/// produced resources by deleting the ones that were produced earlier and are not produced right now.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Meta for marking types for native resources that should be supported by resource logic.
struct kan_resource_resource_type_meta_t
{
    /// \brief If true, resource is considered as root for resource packing mechanism.
    /// \details Only root resources and resources recursively referenced by them are packed by resource builder.
    kan_bool_t root;
};

/// \brief Contains information about loaded compilation dependency and its data.
struct kan_resource_compilation_dependency_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    const void *data;
};

/// \brief Defines interface of byproduct registration functor. Returns registered byproduct resource name.
/// \details Given byproduct data can be userspace and can be allocated on stack. If registration creates new byproduct,
///          it will move the data into separate allocation through move function.
///          Byproduct data is guaranteed to be reset either through kan_resource_byproduct_move_functor_t
///          or kan_resource_byproduct_reset_functor_t if it is present.
typedef kan_interned_string_t (*kan_resource_compilation_register_byproduct_functor_t) (
    kan_functor_user_data_t interface_user_data, kan_interned_string_t byproduct_type_name, void *byproduct_data);

/// \brief Defines whole state of compilation routine.
struct kan_resource_compile_state_t
{
    /// \brief Pointer to the raw instance of compilable type.
    void *input_instance;

    /// \brief Pointer to the pre-initialized instance of compilation output type.
    void *output_instance;

    /// \brief Pointer to platform configuration if required for compilation.
    void *platform_configuration;

    /// \brief Time in `kan_precise_time_get_elapsed_nanoseconds` format after which compilation should return.
    /// \details If compilation isn't finished and needs another run, return KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS.
    kan_time_size_t deadline;

    /// \brief Pointer to the pre-initialized user state structure if required for compilation.
    void *user_state;

    /// \brief True of compiling during runtime, not from tool.
    kan_bool_t runtime_compilation;

    /// \brief Count of dependencies in ::dependencies array.
    kan_instance_size_t dependencies_count;

    /// \brief Array with all dependencies (loaded referenced resources) that are marked as needed for compilation
    ///        in raw or compiled state.
    struct kan_resource_compilation_dependency_t *dependencies;

    /// \brief User data for compilation interface function calls.
    kan_functor_user_data_t interface_user_data;

    /// \brief Function for registering byproducts. Returns registered byproduct resource name.
    kan_resource_compilation_register_byproduct_functor_t register_byproduct;
};

/// \brief Results of compilation functor execution.
enum kan_resource_compile_result_t
{
    /// \brief Stopped by a deadline, needs to be called again.
    KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS = 0u,

    /// \brief Failed to compile resource.
    KAN_RESOURCE_PIPELINE_COMPILE_FAILED,

    /// \brief Successfully compiled resource.
    KAN_RESOURCE_PIPELINE_COMPILE_FINISHED,
};

/// \brief Declares signature for resource compilation functor.
typedef enum kan_resource_compile_result_t (*kan_resource_compile_functor_t) (
    struct kan_resource_compile_state_t *state);

/// \brief Meta that should be added to resource and byproduct types that can be compiled.
struct kan_resource_compilable_meta_t
{
    /// \brief Name of the compilation output type.
    const char *output_type_name;

    /// \brief Name of the platform configuration type. Optional, leave NULL if no configuration needed.
    const char *configuration_type_name;

    /// \brief Name of the compilation user state type. Optional, leave NULL if no configuration needed.
    const char *state_type_name;

    /// \brief Functor that implements compilation routine.
    kan_resource_compile_functor_t functor;
};

/// \brief Declares signature for byproduct hash function.
typedef kan_hash_t (*kan_resource_byproduct_hash_functor_t) (void *byproduct);

/// \brief Declares signature for byproduct equality check.
typedef kan_bool_t (*kan_resource_byproduct_is_equal_functor_t) (const void *first, const void *second);

/// \brief Declares signature for byproduct move function.
typedef void (*kan_resource_byproduct_move_functor_t) (void *target, void *source);

/// \brief Declares signature for byproduct reset function.
typedef void (*kan_resource_byproduct_reset_functor_t) (void *byproduct);

/// \brief Meta for marking types that can be byproducts of the resource compilation.
struct kan_resource_byproduct_type_meta_t
{
    /// \brief Byproduct hash function pointer.
    /// \details Hash function is used to map equal byproducts than can replace each other.
    ///          Leave NULL to use kan_reflection_hash_struct.
    kan_resource_byproduct_hash_functor_t hash;

    /// \brief Byproduct equality check function.
    /// \details When byproducts have the same hash, resource logic should check whether byproducts are equal and
    ///          therefore can replace each other to avoid resource duplication.
    ///          Leave NULL to use kan_reflection_are_structs_equal.
    kan_resource_byproduct_is_equal_functor_t is_equal;

    /// \brief Byproduct move function.
    /// \details Byproduct move function is used to move byproduct data from user allocation into resource tool
    ///          allocation. Its primary goal is to let user reuse one byproduct allocation (possibly even on stack)
    ///          and move data to the new allocations automatically when byproduct is really new.
    ///          Leave NULL to use kan_reflection_move_struct.
    kan_resource_byproduct_move_functor_t move;

    /// \brief Byproduct reset function.
    /// \details Used to automatically reset given byproduct if registration detected that equal byproduct already
    ///          exists. Leave NULL to use kan_reflection_reset_struct.
    kan_resource_byproduct_reset_functor_t reset;
};

/// \brief Defines format of platform configuration file.
struct kan_resource_platform_configuration_t
{
    /// \brief Name and extension of the parent configuration file if any. For example, `pc_base.rd`.
    /// \details Is allowed to contain `/` and `..` and represents relative path from this platform configuration file
    ///          directory to the parent configuration file. Child configuration file inherits all parent configuration
    ///          and applies its patches on top of it, therefore making inheritance hierarchy of configuration files.
    kan_interned_string_t parent;

    /// \brief List of patches that define different types of platform configurations.
    /// \details Should not contain duplicate types as it may result in non-obvious overrides.
    /// \meta reflection_dynamic_array_type = "kan_reflection_patch_t"
    struct kan_dynamic_array_t configuration;
};

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_init (
    struct kan_resource_platform_configuration_t *instance);

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_shutdown (
    struct kan_resource_platform_configuration_t *instance);

/// \brief Describes whether and how reference is used in resource compilation routine if any.
enum kan_resource_compilation_usage_type_t
{
    /// \brief Referenced object is not needed for resource compilation.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED = 0u,

    /// \brief Referenced object raw data is needed for resource compilation.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,

    /// \brief Compiled version of referenced object is needed for resource compilation.
    /// \details Resource object should have `kan_resource_compilable_meta_t` meta.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

/// \brief Meta for marking fields of native resources that point to other resources or byproducts by their name.
/// \invariant Field type is 'kan_interned_string_t'.
struct kan_resource_reference_meta_t
{
    const char *type;
    enum kan_resource_compilation_usage_type_t compilation_usage;
};

/// \brief Stores pointer to field and its `kan_resource_reference_meta_t` for
///        `kan_resource_reference_type_info_node_t`.
struct kan_resource_reference_field_info_t
{
    /// \brief Field with reference if has interned string archetype or
    ///        transitional field (has interned strings with references).
    const struct kan_reflection_field_t *field;

    kan_bool_t is_leaf_field;
    kan_interned_string_t type;
    enum kan_resource_compilation_usage_type_t compilation_usage;
};

/// \brief Contains information needed for `kan_resource_detect_references` for a particular type.
struct kan_resource_reference_type_info_node_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_hash_storage_node_t node;

    kan_interned_string_t type_name;

    /// \brief Lists fields that may contain references (including structs that may contain references in their fields).
    /// \meta reflection_dynamic_array_type = "struct kan_resource_reference_field_info_t"
    struct kan_dynamic_array_t fields_to_check;

    /// \brief List of resource types that are able to reference resources of this type.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t referencer_types;

    kan_bool_t is_resource_type;
    kan_bool_t contains_patches;
};

/// \brief Contains processed reflection data for reference detection.
struct kan_resource_reference_type_info_storage_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_hash_storage_t scanned_types;

    /// \brief Contains list of resource type names that can reference third party resources.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t third_party_referencers;

    kan_reflection_registry_t registry;

    kan_allocation_group_t scanned_allocation_group;
};

/// \brief Builds reference type info storage from given registry, allocating it in given allocation group.
RESOURCE_PIPELINE_API void kan_resource_reference_type_info_storage_build (
    struct kan_resource_reference_type_info_storage_t *storage,
    kan_reflection_registry_t registry,
    kan_allocation_group_t allocation_group);

/// \brief Queries resource type info node for particular type.
RESOURCE_PIPELINE_API const struct kan_resource_reference_type_info_node_t *
kan_resource_reference_type_info_storage_query (struct kan_resource_reference_type_info_storage_t *storage,
                                                kan_interned_string_t type_name);

/// \brief Frees resource type info storage and its resources.
RESOURCE_PIPELINE_API void kan_resource_reference_type_info_storage_shutdown (
    struct kan_resource_reference_type_info_storage_t *storage);

/// \brief Describes reference that was detected during scan for references.
struct kan_resource_detected_reference_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    enum kan_resource_compilation_usage_type_t compilation_usage;
};

/// \brief Container for detected references.
struct kan_resource_detected_reference_container_t
{
    /// \meta reflection_dynamic_array_type = "struct kan_resource_detected_reference_t"
    struct kan_dynamic_array_t detected_references;
};

RESOURCE_PIPELINE_API void kan_resource_detected_reference_container_init (
    struct kan_resource_detected_reference_container_t *instance);

RESOURCE_PIPELINE_API void kan_resource_detected_reference_container_shutdown (
    struct kan_resource_detected_reference_container_t *instance);

/// \brief Detects all references in given referencer instance and outputs them to given container.
RESOURCE_PIPELINE_API void kan_resource_detect_references (
    struct kan_resource_reference_type_info_storage_t *storage,
    kan_interned_string_t referencer_type_name,
    const void *referencer_data,
    struct kan_resource_detected_reference_container_t *output_container);

/// \brief Returns allocation group that should be used for import rules.
RESOURCE_PIPELINE_API kan_allocation_group_t kan_resource_import_rule_get_allocation_group (void);

/// \brief Describes root directory that is used as prefix for import rule source path.
enum kan_resource_import_source_path_root_t
{
    /// \brief Source path is relative to the directory in which import rule is stored.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_FILE_DIRECTORY = 0u,

    /// \brief Source path is relative to the resource directory root in which import rule is stored.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_RESOURCE_DIRECTORY,

    /// \brief Source path is relative to the application CMake source directory.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_APPLICATION_SOURCE,

    /// \brief Source path is relative to the CMake project source of project to which application belongs.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_PROJECT_SOURCE,

    /// \brief Source path is relative to CMake source directory (source for the whole generation).
    KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_CMAKE_SOURCE,
};

/// \brief Describes how import rule source path should be used.
enum kan_resource_import_source_path_rule_t
{
    /// \brief Rule has only one source file which can be found by exact source path.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_EXACT = 0u,

    /// \brief Rule has multiple source files which are found in directory hierarchy and source path points to the root
    ///        directory of the hierarchy.
    KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_HIERARCHY,
};

/// \brief Structure for serializing import input checksum.
/// \details We use serializable structure in order to properly serialize checksum bits because
///          readable data doesn't support values that cannot fit into 32-bit integer.
struct kan_resource_import_serializable_checksum_t
{
    kan_serialized_offset_t bits_1;
    kan_serialized_offset_t bits_2;
};

/// \brief Describes concrete input file used during last execution of import rule and its products.
struct kan_resource_import_input_t
{
    char *source_path;

    union
    {
        /// \brief Checksum is used to detected whether source file was changed.
        struct kan_resource_import_serializable_checksum_t serializable_checksum;

        // \c_interface_scanner_disable
        kan_file_size_t checksum;
        // \c_interface_scanner_enable
    };

    /// \brief Relative (to the rule) paths of outputs that were produced during last import execution.
    /// \meta reflection_dynamic_array_type = "char *"
    struct kan_dynamic_array_t outputs;
};

RESOURCE_PIPELINE_API void kan_resource_import_input_init (struct kan_resource_import_input_t *instance);

RESOURCE_PIPELINE_API void kan_resource_import_input_shutdown (struct kan_resource_import_input_t *instance);

/// \brief Describes one import rule.
/// \details Import rules are used to import one or more similar files using the same configuration.
struct kan_resource_import_rule_t
{
    enum kan_resource_import_source_path_root_t source_path_root;
    enum kan_resource_import_source_path_rule_t source_path_rule;
    char *source_path;

    /// \brief If not NULL, only files ending with this extension will be accepted.
    /// \details Ignored when ::source_path_rule is KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_EXACT.
    kan_interned_string_t extension_filter;

    /// \brief Describes import configuration. Should always be present as it is used to select import functor.
    kan_reflection_patch_t configuration;

    /// \brief Describes inputs and outputs during last import procedure.
    /// \meta reflection_dynamic_array_type = "struct kan_resource_import_input_t"
    struct kan_dynamic_array_t last_import;
};

RESOURCE_PIPELINE_API void kan_resource_import_rule_init (struct kan_resource_import_rule_t *instance);

RESOURCE_PIPELINE_API void kan_resource_import_rule_shutdown (struct kan_resource_import_rule_t *instance);

/// \brief Type of function for registering resources produced from import.
typedef kan_bool_t (*kan_resource_import_interface_produce) (kan_functor_user_data_t user_data,
                                                             const char *relative_path,
                                                             kan_interned_string_t type_name,
                                                             void *data);

/// \brief Structure that contains interface functions for the import functor to report results.
struct kan_resource_import_interface_t
{
    kan_functor_user_data_t user_data;
    kan_resource_import_interface_produce produce;
};

/// \brief Type of import functor function.
/// \warning Returning failure (KAN_FALSE) from import functor does not revert already produced resources.
typedef kan_bool_t (*kan_resource_import_functor) (struct kan_stream_t *input_stream,
                                                   const char *input_path,
                                                   kan_reflection_registry_t registry,
                                                   void *configuration,
                                                   struct kan_resource_import_interface_t *interface);

/// \brief Meta for marking import rule configuration types.
///        Provides information needed to start import using configuration.
struct kan_resource_import_configuration_type_meta_t
{
    /// \brief Functor that will be called for every input file separately, possibly from multiple threads at once.
    kan_resource_import_functor functor;

    /// \brief True if source files can be compared using checksum.
    kan_bool_t allow_checksum;
};

KAN_C_HEADER_END
