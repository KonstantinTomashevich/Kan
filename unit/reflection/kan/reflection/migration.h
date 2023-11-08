#pragma once

#include <reflection_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides tools for migrating struct instances and patches from one registry to another.
///
/// \par Migration
/// \parblock
/// Migration is a process of conversion from one registry to another equivalent registry with some changes. Classic use
/// case for migration is hot reload of types: we've reloaded code and therefore need to migrate data to new formats.
/// \endparblock
///
/// \par Migration seed
/// \parblock
/// Migration seed contains basic descriptions on how to port data from one registry to another. For example, enum
/// value remaps and field remaps. Main goal of a seed is to prepare information for building actual migration code.
/// Seed structures are transparent so other modules might build custom migrators for custom use cases if needed.
/// Both registries must still exist in the memory in order for seed to be valid.
/// \endparblock
///
/// \par Migrator
/// \parblock
/// Migrator is an object that actually performs migration operations. It usually contains optimized bytecode that
/// allows migration to be much faster than it would be if it was done in a straightforward manner. Also it might
/// depend on migration seed data, therefore seed should not be destroyed before migrator destruction.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// - As long as registries remain in read-only mode, migration seed can be safely built in a separate thread.
/// - As long as registries remain in read-only mode, migrator can be built in a separate thread.
/// - Structure instance migration keeps migrator read-only, therefore it is allowed to migrate several struct instances
///   in several threads at once.
/// - Patch migration modifies both registries, therefore it is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_reflection_migration_seed_t;

/// \brief Describes what will happen with structure or enumeration during migration.
enum kan_reflection_migration_status_t
{
    /// \brief Structure or enumeration must be migrated in order to be usable.
    KAN_REFLECTION_MIGRATION_NEEDED,

    /// \brief Structure or enumeration is the same in both registries and does not need to be migrated.
    KAN_REFLECTION_MIGRATION_NOT_NEEDED,

    /// \brief Structure or enumeration is absent in target registry.
    KAN_REFLECTION_MIGRATION_REMOVED,
};

/// \brief Describes information about enumeration migration.
struct kan_reflection_enum_migration_seed_t
{
    enum kan_reflection_migration_status_t status;
    const struct kan_reflection_enum_value_t *value_remap[];
};

/// \brief Describes information about structure migration.
struct kan_reflection_struct_migration_seed_t
{
    enum kan_reflection_migration_status_t status;
    const struct kan_reflection_field_t *field_remap[];
};

/// \brief Builds new seed for migration from source to target registry.
REFLECTION_API kan_reflection_migration_seed_t kan_reflection_migration_seed_build (
    kan_reflection_registry_t source_registry, kan_reflection_registry_t target_registry);

/// \brief Queries seed information for enumeration with given name.
REFLECTION_API const struct kan_reflection_enum_migration_seed_t *kan_reflection_migration_seed_get_for_enum (
    kan_reflection_migration_seed_t seed, kan_interned_string_t type_name);

/// \brief Queries seed information for structure with given name.
REFLECTION_API const struct kan_reflection_struct_migration_seed_t *kan_reflection_migration_seed_get_for_struct (
    kan_reflection_migration_seed_t seed, kan_interned_string_t type_name);

/// \brief Destroys given seed and frees its resources.
REFLECTION_API void kan_reflection_migration_seed_destroy (kan_reflection_migration_seed_t seed);

typedef uint64_t kan_reflection_struct_migrator_t;

/// \brief Builds new migrator from given migration seed.
REFLECTION_API kan_reflection_struct_migrator_t
kan_reflection_struct_migrator_build (kan_reflection_migration_seed_t seed);

/// \brief Migrates data from given source instance to given target instance, that must be properly allocated and
///        initialized, using migration logic for structures with given type name.
REFLECTION_API void kan_reflection_struct_migrator_migrate_instance (kan_reflection_struct_migrator_t migrator,
                                                                     kan_interned_string_t type_name,
                                                                     void *source,
                                                                     void *target);

/// \brief Migrates all patches from source registry to target registry.
/// \details Patch handles are preserved the same.
REFLECTION_API void kan_reflection_struct_migrator_migrate_patches (kan_reflection_struct_migrator_t migrator,
                                                                    kan_reflection_registry_t source_registry,
                                                                    kan_reflection_registry_t target_registry);

/// \brief Destroys given migrator and frees its resources.
REFLECTION_API void kan_reflection_struct_migrator_destroy (kan_reflection_struct_migrator_t migrator);

KAN_C_HEADER_END
