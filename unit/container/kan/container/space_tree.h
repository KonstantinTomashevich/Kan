#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Contains implementation of multi-dimensional space subdivision tree.
///
/// \par Definition
/// \parblock
/// Space tree is a tree that subdivides multi-dimensional limited space and provides API for faster search of
/// intersections through subdivision. This implementation is oriented to be as open as possible: it manages nodes
/// of space tree without requiring user to explicitly add coordinates to sub nodes. Therefore, it's API only covers
/// search of nodes where intersections are possible and user is responsible to search intersections in these nodes.
/// This policy makes space tree as lightweight as possible and makes it possible to avoid data duplication.
/// But, of course, makes space tree usage a bit less convenient.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// Space tree manages allocation and deallocation of its nodes, but does not manage allocation and deallocation
/// of user-provided sub nodes. Therefore, user must care about sub nodes lifetime. This makes it possible to implement
/// space tree as sub node type agnostic data structure, but passes some responsibility to the user side.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Tree can be allocated everywhere as `kan_space_tree_t` and then initialized using 'kan_space_tree_init'.
///
/// User data is contained inside tree as sub nodes. User should take care of allocation and deallocation of sub nodes.
/// Every user sub node must start with kan_space_tree_sub_node_t structure, for example:
///
/// ```c
/// struct my_node_t
/// {
///     struct kan_space_tree_sub_node_t node;
///     // User fields go after this line.
/// };
/// ```
///
/// To insert sub nodes into tree insertion iteration should be used. For example:
///
/// ```c
/// const double min[] = {min_x, min_y, min_z};
/// const double max[] = {max_x, max_y, max_z};
///
/// struct kan_space_tree_insertion_iterator_t iterator = kan_space_tree_insertion_start (space_tree, min, max);
///
/// // Due to the nature of octree, we might need to insert multiple sub nodes into multiple nodes in order to represent
/// // required axis aligned bounding shape in a correct and optimized way. Therefore, we need this cycle.
/// while (!kan_space_tree_insertion_is_finished (&iterator))
/// {
///     // Allocate and create sub node here.
///     // Then insert it and move iterator.
///     kan_space_tree_insertion_insert_and_move (space_tree, &iterator, &sub_node->node);
/// }
/// ```
///
/// To query for axis aligned bounding shape intersection shape iteration should be used. For example:
///
/// ```c
/// const double min[] = {min_x, min_y, min_z};
/// const double max[] = {max_x, max_y, max_z};
///
/// struct kan_space_tree_shape_iterator_t iterator = kan_space_tree_shape_start (space_tree, min, max);
///
/// // Due to the nature of octree, intersecting sub nodes might be inside different tree nodes and we need to iterate
/// // all of them.
/// while (!kan_space_tree_shape_is_finished (&iterator))
/// {
///     struct kan_space_tree_node_t *node = iterator.current_node;
///     struct kan_space_tree_sub_node_t *sub_node = node->first_sub_node;
///
///     while (sub_node)
///     {
///         const double node_min[] = {/* Fill min coordinates. */};
///         const double node_max[] = {/* Fill max coordinates. */};
///
///         if (kan_check_if_bounds_intersect (space_tree->dimension_count, min, max, node_min, node_max))
///         {
///             // Hooray, we found intersection!
///         }
///
///         sub_node = sub_node->next;
///     }
///
///     kan_space_tree_shape_move_to_next_node (space_tree, &iterator);
/// }
/// ```
///
/// To query for intersection between ray and axis aligned bounding shapes ray iteration should be used. For example:
///
/// ```c
/// const double origin[] = {origin_x, origin_y, origin_z};
/// const double direction[] = {direction_x, direction_y, direction_z};
///
/// struct kan_space_tree_ray_iterator_t iterator =
///     kan_space_tree_ray_start (space_tree, origin, direction, max_time);
///
/// // Due to the nature of octree, intersecting sub nodes might be inside different tree nodes and we need to iterate
/// // all of them.
/// while (!kan_space_tree_ray_is_finished (&iterator))
/// {
///     struct kan_space_tree_node_t *node = iterator.current_node;
///     struct kan_space_tree_sub_node_t *sub_node = node->first_sub_node;
///
///     while (sub_node)
///     {
///         const double node_min[] = {/* Fill min coordinates. */};
///         const double node_max[] = {/* Fill max coordinates. */};
///
///         struct kan_ray_intersection_output_t output = kan_check_if_ray_and_bounds_intersect (
///             space_tree->dimension_count, node_min, node_max, origin, direction);
///
///         if (output.hit && output.time <= max_time)
///         {
///             // Hooray, we found intersection!
///         }
///
///         sub_node = sub_node->next;
///     }
///
///     kan_space_tree_ray_move_to_next_node (space_tree, &iterator);
/// }
/// ```
///
/// Any sub node can be deleted from tree by calling `kan_space_tree_delete`. But beware that this operation
/// modifies tree structure and therefore breaks tree iterators.
///
/// To delete tree and free its resources, use `kan_space_tree_shutdown` function. But keep in mind that tree does
/// not control sub node allocation and therefore sub nodes must be deallocated prior to tree destruction.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// - Query iterators like space iterator and ray iterator do not modify the tree and therefore are thread safe.
/// - Insertion iterator modifies the tree structure and therefore is not thread safe.
/// - Deletion modifies the tree structure and therefore is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

_Static_assert (KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS <= 4u,
                "Current implementation is optimized for 4 or less dimensions.");

/// \brief Describes path from root to appropriate leaf node as combination of dimension-specific roads.
struct kan_space_tree_quantized_path_t
{
    union
    {
        uint16_t roads[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
        uint64_t combined;
    };
};

/// \brief Base structure for space tree sub nodes.
struct kan_space_tree_sub_node_t
{
    struct kan_space_tree_sub_node_t *next;
    struct kan_space_tree_sub_node_t *previous;
};

/// \brief Describes space tree node structure.
struct kan_space_tree_node_t
{
    struct kan_space_tree_node_t *parent;
    uint8_t height;
    struct kan_space_tree_sub_node_t *first_sub_node;

    /// \brief Array of children, size depends on dimension count of tree.
    /// \warning Nodes with last level height of the tree do not have this array at all!
    struct kan_space_tree_node_t *children[];
};

/// \brief Root structure of space tree implementation.
struct kan_space_tree_t
{
    struct kan_space_tree_node_t *root;
    kan_allocation_group_t allocation_group;
    uint16_t dimension_count;
    uint16_t last_level_height;
    double global_min;
    double global_max;
};

/// \brief Structure of iterator used for querying intersections with axis aligned bounding shapes.
struct kan_space_tree_shape_iterator_t
{
    struct kan_space_tree_quantized_path_t min_path;
    struct kan_space_tree_quantized_path_t max_path;
    struct kan_space_tree_quantized_path_t current_path;

    /// \brief Current node from which user can take sub nodes for further querying.
    struct kan_space_tree_node_t *current_node;
};

/// \brief Structure of iterator used for insertion of axis aligned bounding shapes.
struct kan_space_tree_insertion_iterator_t
{
    struct kan_space_tree_shape_iterator_t base;
    uint16_t target_height;
};

/// \brief Structure of iterator used for querying ray intersections with inserted axis aligned bounding shapes.
struct kan_space_tree_ray_iterator_t
{
    struct kan_space_tree_quantized_path_t current_path;
    struct kan_space_tree_quantized_path_t next_path;

    /// \brief Current node from which user can take sub nodes for further querying.
    struct kan_space_tree_node_t *current_node;

    double position[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    double direction[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    double travelled_time;
    double max_time;
};

/// \brief Initializes given space tree with given parameters.
///
/// \param tree Pointer for tree to initialize.
/// \param allocation_group Allocation group for tree internal allocations.
/// \param dimension_count Count of space dimensions. Must not be higher that KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS.
/// \param global_min Global minimum limit for all dimensions.
/// \param global_max Global maximum limit for all dimensions.
/// \param target_leaf_cell_size Target size of a leaf node.
///                              Used to calibrate space tree height and make it more optimal for game levels.
CONTAINER_API void kan_space_tree_init (struct kan_space_tree_t *tree,
                                        kan_allocation_group_t allocation_group,
                                        uint64_t dimension_count,
                                        double global_min,
                                        double global_max,
                                        double target_leaf_cell_size);

/// \brief Starts iteration that aims to insert sub node to every node that should contain axis aligned bounding shape
///        with given min and max coordinates.
CONTAINER_API struct kan_space_tree_insertion_iterator_t kan_space_tree_insertion_start (struct kan_space_tree_t *tree,
                                                                                         const double *min_sequence,
                                                                                         const double *max_sequence);

/// \brief Inserts given sub node into tree and moves to the next node for the insertion.
/// \invariant kan_space_tree_insertion_is_finished is KAN_FALSE.
CONTAINER_API void kan_space_tree_insertion_insert_and_move (struct kan_space_tree_t *tree,
                                                             struct kan_space_tree_insertion_iterator_t *iterator,
                                                             struct kan_space_tree_sub_node_t *sub_node);

/// \brief Whether given insertion iteration is finished.
static inline kan_bool_t kan_space_tree_insertion_is_finished (struct kan_space_tree_insertion_iterator_t *iterator)
{
    return !iterator->base.current_node;
}

/// \brief Starts iteration that aims to query intersections between given and inserted axis aligned bounding shapes.
CONTAINER_API struct kan_space_tree_shape_iterator_t kan_space_tree_shape_start (struct kan_space_tree_t *tree,
                                                                                 const double *min_sequence,
                                                                                 const double *max_sequence);

/// \brief Moves shape iterator to the next node that may contain intersections.
CONTAINER_API void kan_space_tree_shape_move_to_next_node (struct kan_space_tree_t *tree,
                                                           struct kan_space_tree_shape_iterator_t *iterator);

/// \brief Whether given shape iteration is finished.
static inline kan_bool_t kan_space_tree_shape_is_finished (struct kan_space_tree_shape_iterator_t *iterator)
{
    return !iterator->current_node;
}

/// \brief Starts iteration that aims to query intersections between given ray and
///        inserted axis aligned bounding shapes.
CONTAINER_API struct kan_space_tree_ray_iterator_t kan_space_tree_ray_start (struct kan_space_tree_t *tree,
                                                                             const double *origin_sequence,
                                                                             const double *direction_sequence,
                                                                             double max_time);

/// \brief Moves ray iterator to the next node that may contain intersections.
CONTAINER_API void kan_space_tree_ray_move_to_next_node (struct kan_space_tree_t *tree,
                                                         struct kan_space_tree_ray_iterator_t *iterator);

/// \brief Whether given ray iteration is finished.
static inline kan_bool_t kan_space_tree_ray_is_finished (struct kan_space_tree_ray_iterator_t *iterator)
{
    return !iterator->current_node;
}

/// \brief Checks whether given axis aligned bounding shape needs to be deleted and
///        re-inserted after its values changed from old to new sequences.
CONTAINER_API kan_bool_t kan_space_tree_is_re_insert_needed (struct kan_space_tree_t *tree,
                                                             const double *old_min,
                                                             const double *old_max,
                                                             const double *new_min,
                                                             const double *new_max);

/// \brief Checks whether given axis aligned bounding shape can be stored as only one sub node.
CONTAINER_API kan_bool_t kan_space_tree_is_contained_in_one_sub_node (struct kan_space_tree_t *tree,
                                                                      const double *min,
                                                                      const double *max);

/// \brief Deletes given sub node from given space tree.
/// \warning Breaks iterators!
CONTAINER_API void kan_space_tree_delete (struct kan_space_tree_t *tree,
                                          struct kan_space_tree_node_t *node,
                                          struct kan_space_tree_sub_node_t *sub_node);

/// \brief Shuts down given space tree and frees its resources.
/// \invariant User must free sub nodes manually before executing this operation.
CONTAINER_API void kan_space_tree_shutdown (struct kan_space_tree_t *tree);

/// \brief Helper for checking for intersection between two axis aligned bounding shapes.
static inline kan_bool_t kan_check_if_bounds_intersect (uint64_t dimension_count,
                                                        const double *first_min,
                                                        const double *first_max,
                                                        const double *second_min,
                                                        const double *second_max)
{
    kan_bool_t no_intersection = KAN_FALSE;
    switch (dimension_count)
    {
    case 4u:
        no_intersection |= second_max[3u] < first_min[3u] || first_max[3u] < second_min[3u];
    case 3u:
        no_intersection |= second_max[2u] < first_min[2u] || first_max[2u] < second_min[2u];
    case 2u:
        no_intersection |= second_max[1u] < first_min[1u] || first_max[1u] < second_min[1u];
    case 1u:
        no_intersection |= second_max[0u] < first_min[0u] || first_max[0u] < second_min[0u];
    }

    return !no_intersection;
}

/// \brief Output structure for kan_check_if_ray_and_bounds_intersect.
struct kan_ray_intersection_output_t
{
    kan_bool_t hit;
    double time;
    double coordinates[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
};

/// \brief Helper for checking for intersection between axis aligned bounding shape and a ray.
static inline struct kan_ray_intersection_output_t kan_check_if_ray_and_bounds_intersect (uint64_t dimension_count,
                                                                                          const double *bounds_min,
                                                                                          const double *bounds_max,
                                                                                          const double *ray_origin,
                                                                                          const double *ray_direction)
{
    KAN_MUTE_UNINITIALIZED_WARNINGS_BEGIN
    // Integration of:
    //     Fast Ray-Box Intersection
    //     by Andrew Woo
    //     from "Graphics Gems", Academic Press, 1990

    struct kan_ray_intersection_output_t result;

    enum quadrant_t
    {
        QUADRANT_RIGHT = 0,
        QUADRANT_LEFT,
        QUADRANT_MIDDLE,
    };

    kan_bool_t inside = KAN_TRUE;
    enum quadrant_t quadrants[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    double candidate_plane[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];

    switch (dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if (ray_origin[DIMENSION] < bounds_min[DIMENSION])                                                             \
        {                                                                                                              \
            quadrants[DIMENSION] = QUADRANT_LEFT;                                                                      \
            candidate_plane[DIMENSION] = bounds_min[DIMENSION];                                                        \
            inside = KAN_FALSE;                                                                                        \
        }                                                                                                              \
        else if (ray_origin[DIMENSION] > bounds_max[DIMENSION])                                                        \
        {                                                                                                              \
            quadrants[DIMENSION] = QUADRANT_RIGHT;                                                                     \
            candidate_plane[DIMENSION] = bounds_max[DIMENSION];                                                        \
            inside = KAN_FALSE;                                                                                        \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            quadrants[DIMENSION] = QUADRANT_MIDDLE;                                                                    \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    if (inside)
    {
        result.hit = KAN_TRUE;
        result.time = 0.0;

        switch (dimension_count)
        {
        case 4u:
            result.coordinates[3u] = ray_origin[3u];
        case 3u:
            result.coordinates[2u] = ray_origin[2u];
        case 2u:
            result.coordinates[1u] = ray_origin[1u];
        case 1u:
            result.coordinates[0u] = ray_origin[0u];
        }

        return result;
    }

    uint64_t target_plane = UINT64_MAX;
    double max_time = -1.0;

    switch (dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if (quadrants[DIMENSION] != QUADRANT_MIDDLE && ray_direction[DIMENSION] != 0.0)                                \
        {                                                                                                              \
            double time = (candidate_plane[DIMENSION] - ray_origin[DIMENSION]) / ray_direction[DIMENSION];             \
            if (time > max_time)                                                                                       \
            {                                                                                                          \
                max_time = time;                                                                                       \
                target_plane = DIMENSION;                                                                              \
            }                                                                                                          \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    if (target_plane == UINT64_MAX)
    {
        result.hit = KAN_FALSE;
        return result;
    }

    switch (dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if (target_plane != DIMENSION)                                                                                 \
        {                                                                                                              \
            result.coordinates[DIMENSION] = ray_origin[DIMENSION] + max_time * ray_direction[DIMENSION];               \
            if (result.coordinates[DIMENSION] < bounds_min[DIMENSION] ||                                               \
                result.coordinates[DIMENSION] > bounds_max[DIMENSION])                                                 \
            {                                                                                                          \
                result.hit = KAN_FALSE;                                                                                \
                return result;                                                                                         \
            }                                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            result.coordinates[DIMENSION] = candidate_plane[DIMENSION];                                                \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    result.hit = KAN_TRUE;
    result.time = max_time;
    return result;
    KAN_MUTE_UNINITIALIZED_WARNINGS_END
}

KAN_C_HEADER_END
