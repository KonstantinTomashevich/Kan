#include <float.h>
#include <stddef.h>
#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/container/space_tree.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

static inline kan_space_tree_road_t quantize (kan_space_tree_floating_t value,
                                              kan_space_tree_floating_t min,
                                              kan_space_tree_floating_t max)
{
    const kan_space_tree_floating_t normalized_value = (value - min) / (max - min);
    const kan_space_tree_floating_t clamped_value = KAN_CLAMP (normalized_value, 0.0, 1.0);
    return (kan_space_tree_road_t) (clamped_value * (kan_space_tree_floating_t) KAN_INT_MAX (kan_space_tree_road_t));
}

KAN_MUTE_UNINITIALIZED_WARNINGS_BEGIN

static inline struct kan_space_tree_quantized_path_t quantize_sequence (struct kan_space_tree_t *tree,
                                                                        const kan_space_tree_floating_t *sequence)
{
    struct kan_space_tree_quantized_path_t path;
    switch (tree->dimension_count)
    {
    case 4u:
        path.roads[3u] = quantize (sequence[3u], tree->global_min, tree->global_max);
    case 3u:
        path.roads[2u] = quantize (sequence[2u], tree->global_min, tree->global_max);
    case 2u:
        path.roads[1u] = quantize (sequence[1u], tree->global_min, tree->global_max);
    case 1u:
        path.roads[0u] = quantize (sequence[0u], tree->global_min, tree->global_max);
    }

    return path;
}

KAN_MUTE_UNINITIALIZED_WARNINGS_END

static inline kan_space_tree_floating_t to_quantized_space (kan_space_tree_floating_t value,
                                                            kan_space_tree_floating_t min,
                                                            kan_space_tree_floating_t max)
{
    const kan_space_tree_floating_t normalized_value = (value - min) / (max - min);
    const kan_space_tree_floating_t clamped_value = KAN_CLAMP (normalized_value, 0.0, 1.0);
    return (clamped_value * (kan_space_tree_floating_t) KAN_INT_MAX (kan_space_tree_road_t));
}

#define FIRST_HEIGHT_SHIFT (sizeof (kan_space_tree_road_t) * 8u - 1u)

static inline kan_space_tree_road_t make_height_mask (kan_space_tree_road_t height)
{
    KAN_ASSERT (height <= FIRST_HEIGHT_SHIFT)
    return (1u << (FIRST_HEIGHT_SHIFT - height));
}

static inline kan_space_tree_road_t node_height_mask (struct kan_space_tree_node_t *node)
{
    return make_height_mask (node->height);
}

static inline kan_space_tree_road_t height_mask_to_root_to_height_mask (kan_space_tree_road_t height_mask)
{
    KAN_ASSERT (height_mask > 0u)
    return ~(height_mask - 1u);
}

static inline uint8_t calculate_child_node_index (struct kan_space_tree_t *tree,
                                                  struct kan_space_tree_node_t *node,
                                                  struct kan_space_tree_quantized_path_t path)
{
    uint8_t index = 0u;
    const kan_space_tree_road_t height_mask = node_height_mask (node);

    switch (tree->dimension_count)
    {
    case 4u:
        index |= ((path.roads[3u] & height_mask) ? 1u : 0u) << 3u;
    case 3u:
        index |= ((path.roads[2u] & height_mask) ? 1u : 0u) << 2u;
    case 2u:
        index |= ((path.roads[1u] & height_mask) ? 1u : 0u) << 1u;
    case 1u:
        index |= ((path.roads[0u] & height_mask) ? 1u : 0u) << 0u;
    }

    return index;
}

static inline void shape_iterator_reset_dimension (struct kan_space_tree_shape_iterator_t *iterator,
                                                   kan_space_tree_road_t dimension_index,
                                                   kan_space_tree_road_t root_to_height_mask,
                                                   kan_space_tree_road_t height_mask,
                                                   kan_space_tree_road_t reversed_height_mask)
{
    const kan_space_tree_road_t masked_current = iterator->current_path.roads[dimension_index] & root_to_height_mask;
    const kan_space_tree_road_t masked_min = iterator->min_path.roads[dimension_index] & root_to_height_mask;

    if (masked_current > masked_min)
    {
        iterator->current_path.roads[dimension_index] &= reversed_height_mask;
    }
    else if (masked_current < masked_min)
    {
        iterator->current_path.roads[dimension_index] |= height_mask;
    }
}

static inline void shape_iterator_reset_dimensions_after (struct kan_space_tree_t *tree,
                                                          struct kan_space_tree_shape_iterator_t *iterator,
                                                          kan_space_tree_road_t after_dimension_index,
                                                          kan_space_tree_road_t root_to_height_mask,
                                                          kan_space_tree_road_t height_mask,
                                                          kan_space_tree_road_t reversed_height_mask)
{
    switch (tree->dimension_count)
    {
    case 4u:
        switch (after_dimension_index + 1u)
        {
        case 1u:
            shape_iterator_reset_dimension (iterator, 1u, root_to_height_mask, height_mask, reversed_height_mask);
        case 2u:
            shape_iterator_reset_dimension (iterator, 2u, root_to_height_mask, height_mask, reversed_height_mask);
        case 3u:
            shape_iterator_reset_dimension (iterator, 3u, root_to_height_mask, height_mask, reversed_height_mask);
        }

        break;

    case 3u:
        switch (after_dimension_index + 1u)
        {
        case 1u:
            shape_iterator_reset_dimension (iterator, 1u, root_to_height_mask, height_mask, reversed_height_mask);
        case 2u:
            shape_iterator_reset_dimension (iterator, 2u, root_to_height_mask, height_mask, reversed_height_mask);
        }

        break;

    case 2u:
        switch (after_dimension_index + 1u)
        {
        case 1u:
            shape_iterator_reset_dimension (iterator, 1u, root_to_height_mask, height_mask, reversed_height_mask);
        }

        break;

    case 1u:
        break;
    }
}

static inline void shape_iterator_reset_all_dimensions (struct kan_space_tree_t *tree,
                                                        struct kan_space_tree_shape_iterator_t *iterator,
                                                        kan_space_tree_road_t root_to_height_mask,
                                                        kan_space_tree_road_t height_mask,
                                                        kan_space_tree_road_t reversed_height_mask)
{
    switch (tree->dimension_count)
    {
    case 4u:
        shape_iterator_reset_dimension (iterator, 3u, root_to_height_mask, height_mask, reversed_height_mask);
    case 3u:
        shape_iterator_reset_dimension (iterator, 2u, root_to_height_mask, height_mask, reversed_height_mask);
    case 2u:
        shape_iterator_reset_dimension (iterator, 1u, root_to_height_mask, height_mask, reversed_height_mask);
    case 1u:
        shape_iterator_reset_dimension (iterator, 0u, root_to_height_mask, height_mask, reversed_height_mask);
    }
}

static inline kan_bool_t shape_iterator_try_step_in_dimension (struct kan_space_tree_t *tree,
                                                               struct kan_space_tree_shape_iterator_t *iterator,
                                                               kan_space_tree_road_t dimension_index,
                                                               kan_space_tree_road_t height_mask,
                                                               kan_space_tree_road_t reversed_height_mask)
{
    const kan_space_tree_road_t root_to_height_mask = height_mask_to_root_to_height_mask (height_mask);
    const kan_bool_t can_increase = (iterator->current_path.roads[dimension_index] & height_mask) == 0u;

    const kan_bool_t want_increase = (iterator->current_path.roads[dimension_index] & root_to_height_mask) <
                                     (iterator->max_path.roads[dimension_index] & root_to_height_mask);

    if (can_increase && want_increase)
    {
        iterator->current_path.roads[dimension_index] |= height_mask;
        shape_iterator_reset_dimensions_after (tree, iterator, dimension_index, root_to_height_mask, height_mask,
                                               reversed_height_mask);
        return KAN_TRUE;
    }

    return KAN_FALSE;
}

static inline kan_bool_t shape_iterator_try_step_on_height (struct kan_space_tree_t *tree,
                                                            struct kan_space_tree_shape_iterator_t *iterator,
                                                            kan_space_tree_road_t height_mask,
                                                            kan_space_tree_road_t reversed_height_mask)
{
    switch (tree->dimension_count)
    {
    case 4u:
        if (shape_iterator_try_step_in_dimension (tree, iterator, 3u, height_mask, reversed_height_mask))
        {
            return KAN_TRUE;
        }

    case 3u:
        if (shape_iterator_try_step_in_dimension (tree, iterator, 2u, height_mask, reversed_height_mask))
        {
            return KAN_TRUE;
        }

    case 2u:
        if (shape_iterator_try_step_in_dimension (tree, iterator, 1u, height_mask, reversed_height_mask))
        {
            return KAN_TRUE;
        }

    case 1u:
        if (shape_iterator_try_step_in_dimension (tree, iterator, 0u, height_mask, reversed_height_mask))
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static inline void shape_iterator_update_is_inner_node (struct kan_space_tree_t *tree,
                                                        struct kan_space_tree_shape_iterator_t *iterator)
{
    if (!iterator->current_node || iterator->current_node->height == 0u)
    {
        iterator->is_inner_node = KAN_FALSE;
        return;
    }

    iterator->is_inner_node = KAN_TRUE;
    const kan_space_tree_road_t mask =
        height_mask_to_root_to_height_mask (make_height_mask (iterator->current_node->height - 1u));

    switch (tree->dimension_count)
    {
    case 4u:
        iterator->is_inner_node &= (iterator->min_path.roads[3u] & mask) < (iterator->current_path.roads[3u] & mask) &&
                                   (iterator->current_path.roads[3u] & mask) < (iterator->max_path.roads[3u] & mask);
    case 3u:
        iterator->is_inner_node &= (iterator->min_path.roads[2u] & mask) < (iterator->current_path.roads[2u] & mask) &&
                                   (iterator->current_path.roads[2u] & mask) < (iterator->max_path.roads[2u] & mask);
    case 2u:
        iterator->is_inner_node &= (iterator->min_path.roads[1u] & mask) < (iterator->current_path.roads[1u] & mask) &&
                                   (iterator->current_path.roads[1u] & mask) < (iterator->max_path.roads[1u] & mask);
    case 1u:
        iterator->is_inner_node &= (iterator->min_path.roads[0u] & mask) < (iterator->current_path.roads[0u] & mask) &&
                                   (iterator->current_path.roads[0u] & mask) < (iterator->max_path.roads[0u] & mask);
    }
}

static inline struct kan_space_tree_node_t *kan_space_tree_node_get_parent (struct kan_space_tree_node_t *node)
{
    if (node->height == 0u)
    {
        return NULL;
    }

    uint8_t *allocation_address = ((uint8_t *) (node - node->index_in_array)) -
                                  offsetof (struct kan_space_tree_node_children_allocation_t, children);
    return ((struct kan_space_tree_node_children_allocation_t *) allocation_address)->parent;
}

static void shape_iterator_next (struct kan_space_tree_t *tree, struct kan_space_tree_shape_iterator_t *iterator)
{
    struct kan_space_tree_node_t *parent_node = NULL;
    while (KAN_TRUE)
    {
        // At the start of cycle iteration, there is 3 possible situations:
        // 1. It is the first cycle iteration and we're continuing tree iterator iteration.
        //    In this case, iterator->current_node is not null, so we're expected to go further to the next node.
        // 2. It is the first cycle iteration and we're starting from scratch.
        //    In this case, iterator->current_node is null, so we're getting the root and moving from root to current.
        // 3. It is second or further cycle iteration, needed because we've stumbled at zero child while following
        //    the current tree path. In this case iterator->current_node is always null and parent_node is never null.
        //    So, we need to go further in order to try to visit other nodes from hierarchy.
        kan_bool_t go_further = KAN_TRUE;

        if (iterator->current_node)
        {
            KAN_ASSERT (!parent_node)
            // If we're in current, then all children of current are visited. Therefore, go to parent.
            parent_node = kan_space_tree_node_get_parent (iterator->current_node);

            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->current_node->height == 0u)
                KAN_ASSERT (iterator->current_node == &tree->root)
                iterator->current_node = NULL;
                shape_iterator_update_is_inner_node (tree, iterator);
                return;
            }
        }

        if (!parent_node)
        {
            // We don't have current, it means that we're either starting from scratch or restarting iteration again.
            parent_node = &tree->root;
            go_further = KAN_FALSE;
        }

        if (go_further)
        {
            KAN_ASSERT (parent_node)
            KAN_ASSERT (parent_node->height < tree->last_level_height)
            const kan_space_tree_road_t height_mask = node_height_mask (parent_node);
            const kan_space_tree_road_t reversed_height_mask = ~height_mask;

            if (!shape_iterator_try_step_on_height (tree, iterator, height_mask, reversed_height_mask))
            {
                // Nothing more to visit in hierarchy, therefore we can visit parent.
                iterator->current_node = parent_node;
                shape_iterator_update_is_inner_node (tree, iterator);
                return;
            }
        }

        uint8_t child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
        while (parent_node->children_allocation)
        {
            struct kan_space_tree_node_t *child_node = &parent_node->children_allocation->children[child_node_index];
            if (child_node->height == tree->last_level_height)
            {
                // Last level -- no children possible.
                iterator->current_node = child_node;
                shape_iterator_update_is_inner_node (tree, iterator);
                return;
            }

            parent_node = child_node;
            const kan_space_tree_road_t child_height_mask = node_height_mask (child_node);
            const kan_space_tree_road_t reversed_child_height_mask = ~child_height_mask;

            shape_iterator_reset_all_dimensions (tree, iterator, height_mask_to_root_to_height_mask (child_height_mask),
                                                 child_height_mask, reversed_child_height_mask);
            child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
        }

        // We've technically reached null node and will be repositioned in next while iteration.
        iterator->current_node = NULL;
    }
}

static inline struct kan_space_tree_node_t *get_or_create_child_node (struct kan_space_tree_t *tree,
                                                                      struct kan_space_tree_node_t *parent_node,
                                                                      kan_space_tree_road_t child_index)
{
    KAN_ASSERT (parent_node->height != tree->last_level_height)
    if (parent_node->children_allocation)
    {
        return &parent_node->children_allocation->children[child_index];
    }

    const kan_space_tree_road_t child_height = parent_node->height + 1u;
    const kan_instance_size_t children_count = 1u << tree->dimension_count;

    struct kan_space_tree_node_children_allocation_t *children_allocation =
        kan_allocate_batched (tree->nodes_allocation_group, sizeof (struct kan_space_tree_node_children_allocation_t) +
                                                                sizeof (struct kan_space_tree_node_t) * children_count);
    children_allocation->parent = parent_node;

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) children_count; ++index)
    {
        struct kan_space_tree_node_t *child_node = &children_allocation->children[index];
        child_node->index_in_array = (uint8_t) index;
        child_node->height = (uint8_t) child_height;
        child_node->sub_nodes_capacity = 0u;
        child_node->sub_nodes_count = 0u;
        child_node->sub_nodes = NULL;
        child_node->children_allocation = NULL;
    }

    parent_node->children_allocation = children_allocation;
    return &parent_node->children_allocation->children[child_index];
}

static void insertion_iterator_next (struct kan_space_tree_t *tree,
                                     struct kan_space_tree_insertion_iterator_t *iterator)
{
    while (KAN_TRUE)
    {
        struct kan_space_tree_node_t *parent_node;
        if (iterator->base.current_node)
        {
            // If we have current, then we're in a middle of iteration and need to step further.

            // If we're in current, then all children of current are visited. Therefore, go to parent.
            parent_node = kan_space_tree_node_get_parent (iterator->base.current_node);

            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->base.current_node->height == 0u)
                KAN_ASSERT (iterator->base.current_node == &tree->root)
                iterator->base.current_node = NULL;
                return;
            }

            KAN_ASSERT (parent_node->height < tree->last_level_height)
            KAN_ASSERT (parent_node->height < iterator->target_height)

            const kan_space_tree_road_t height_mask = node_height_mask (parent_node);
            const kan_space_tree_road_t reversed_height_mask = ~height_mask;

            if (!shape_iterator_try_step_on_height (tree, &iterator->base, height_mask, reversed_height_mask))
            {
                iterator->base.current_node = parent_node;
                continue;
            }
        }
        else
        {
            // We don't have current, it means that we're either starting from scratch or restarting iteration again.
            parent_node = &tree->root;
        }

        uint8_t child_node_index = calculate_child_node_index (tree, parent_node, iterator->base.current_path);
        struct kan_space_tree_node_t *child_node = get_or_create_child_node (tree, parent_node, child_node_index);

        while (child_node->height < iterator->target_height)
        {
            parent_node = child_node;
            const kan_space_tree_road_t child_height_mask = node_height_mask (child_node);
            const kan_space_tree_road_t reversed_child_height_mask = ~child_height_mask;

            shape_iterator_reset_all_dimensions (tree, &iterator->base,
                                                 height_mask_to_root_to_height_mask (child_height_mask),
                                                 child_height_mask, reversed_child_height_mask);
            child_node_index = calculate_child_node_index (tree, parent_node, iterator->base.current_path);
            child_node = get_or_create_child_node (tree, parent_node, child_node_index);
        }

        iterator->base.current_node = child_node;
        return;
    }
}

struct ray_target_t
{
    kan_space_tree_road_t road;
    kan_space_tree_floating_t time;
    kan_bool_t out_of_bounds;
};

#define FUNCTION_GET_RAY_TARGET_IN_DIMENSION(DIRECTION_NAME, DIRECTION_SIGN)                                           \
    static inline struct ray_target_t get_ray_##DIRECTION_NAME##_in_dimension (                                        \
        struct kan_space_tree_ray_iterator_t *iterator, kan_space_tree_road_t dimension_index,                         \
        kan_space_tree_road_t height_mask, kan_space_tree_road_t height_to_root_mask)                                  \
    {                                                                                                                  \
        struct ray_target_t result;                                                                                    \
        kan_space_tree_floating_t border_value;                                                                        \
                                                                                                                       \
        if (DIRECTION_SIGN iterator->direction[dimension_index] > (kan_floating_t) 0.0)                                \
        {                                                                                                              \
            const kan_space_tree_road_t masked_current =                                                               \
                iterator->current_path.roads[dimension_index] & height_to_root_mask;                                   \
            result.road = masked_current + height_mask;                                                                \
                                                                                                                       \
            if (result.road < masked_current)                                                                          \
            {                                                                                                          \
                /* Overflow. */                                                                                        \
                border_value = (kan_space_tree_floating_t) KAN_INT_MAX (kan_space_tree_road_t);                        \
                result.out_of_bounds = KAN_TRUE;                                                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                border_value = (kan_space_tree_floating_t) result.road;                                                \
                result.out_of_bounds = KAN_FALSE;                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        else if (DIRECTION_SIGN iterator->direction[dimension_index] < (kan_floating_t) 0.0)                           \
        {                                                                                                              \
            const kan_space_tree_road_t masked_current =                                                               \
                iterator->current_path.roads[dimension_index] | ~height_to_root_mask;                                  \
            result.road = masked_current - height_mask;                                                                \
                                                                                                                       \
            if (result.road > masked_current)                                                                          \
            {                                                                                                          \
                /* Underflow. */                                                                                       \
                border_value = (kan_floating_t) 0.0;                                                                   \
                result.out_of_bounds = KAN_TRUE;                                                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                border_value = (kan_space_tree_floating_t) result.road;                                                \
                result.out_of_bounds = KAN_FALSE;                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            result.time = DBL_MAX;                                                                                     \
            result.out_of_bounds = KAN_TRUE;                                                                           \
            return result;                                                                                             \
        }                                                                                                              \
                                                                                                                       \
        const kan_space_tree_floating_t distance_to_border = border_value - iterator->position[dimension_index];       \
        result.time = distance_to_border / DIRECTION_SIGN iterator->direction[dimension_index];                        \
        KAN_ASSERT (result.time >= 0.0)                                                                                \
        return result;                                                                                                 \
    }

FUNCTION_GET_RAY_TARGET_IN_DIMENSION (next, +)
FUNCTION_GET_RAY_TARGET_IN_DIMENSION (previous, -)
#undef FUNCTION_GET_RAY_TARGET_IN_DIMENSION

struct ray_target_and_dimension_t
{
    kan_space_tree_road_t dimension;
    kan_space_tree_road_t target;
    kan_space_tree_floating_t time;
    kan_bool_t out_of_bounds;
};

#define FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE(NAME, DIMENSION)                                                   \
    case (DIMENSION + 1u):                                                                                             \
    {                                                                                                                  \
        struct ray_target_t result =                                                                                   \
            get_ray_##NAME##_in_dimension (iterator, DIMENSION, height_mask, height_to_root_mask);                     \
        if (result.time < smallest.time)                                                                               \
        {                                                                                                              \
            smallest.dimension = DIMENSION;                                                                            \
            smallest.target = result.road;                                                                             \
            smallest.time = result.time;                                                                               \
            smallest.out_of_bounds = result.out_of_bounds;                                                             \
        }                                                                                                              \
    }

#define FUNCTION_CALCULATE_RAY_SMALLEST_TARGET(NAME)                                                                   \
    static inline struct ray_target_and_dimension_t calculate_ray_smallest_##NAME (                                    \
        struct kan_space_tree_t *tree, struct kan_space_tree_ray_iterator_t *iterator,                                 \
        kan_space_tree_road_t height_mask, kan_space_tree_road_t height_to_root_mask)                                  \
    {                                                                                                                  \
        struct ray_target_and_dimension_t smallest = {KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS + 1u, 0u, DBL_MAX,       \
                                                      KAN_TRUE};                                                       \
        switch (tree->dimension_count)                                                                                 \
        {                                                                                                              \
            FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE (NAME, 3u)                                                     \
            FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE (NAME, 2u)                                                     \
            FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE (NAME, 1u)                                                     \
            FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE (NAME, 0u)                                                     \
        }                                                                                                              \
                                                                                                                       \
        return smallest;                                                                                               \
    }

FUNCTION_CALCULATE_RAY_SMALLEST_TARGET (next)
FUNCTION_CALCULATE_RAY_SMALLEST_TARGET (previous)

#undef FUNCTION_CALCULATE_RAY_SMALLEST_TARGET
#undef FUNCTION_CALCULATE_RAY_SMALLEST_TARGET_CASE

static inline void ray_calculate_previous_path_on_level (struct kan_space_tree_t *tree,
                                                         struct kan_space_tree_ray_iterator_t *iterator)
{
    if (!iterator->current_node || iterator->current_node->height == 0u ||
        // No need to spend time on calculations if we don't have sub nodes anyway.
        !iterator->current_node->sub_nodes)
    {
        iterator->has_previous_path_on_level = KAN_FALSE;
        return;
    }

    const kan_space_tree_road_t height_mask = make_height_mask (iterator->current_node->height - 1u);
    const kan_space_tree_road_t height_to_root_mask = height_mask_to_root_to_height_mask (height_mask);

    struct ray_target_and_dimension_t smallest =
        calculate_ray_smallest_previous (tree, iterator, height_mask, height_to_root_mask);

    if (smallest.out_of_bounds)
    {
        // Previous is out of bounds, therefore there is no previous.
        iterator->has_previous_path_on_level = KAN_FALSE;
        return;
    }

    if (iterator->travelled_time < smallest.time)
    {
        // Never been so long back, therefore no previous.
        iterator->has_previous_path_on_level = KAN_FALSE;
        return;
    }

    iterator->has_previous_path_on_level = KAN_TRUE;
    iterator->previous_path_on_level.roads[smallest.dimension] = smallest.target;

    switch (tree->dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if (DIMENSION != smallest.dimension)                                                                           \
        {                                                                                                              \
            iterator->previous_path_on_level.roads[DIMENSION] = iterator->current_path.roads[DIMENSION];               \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }
}

static void ray_iterator_next (struct kan_space_tree_t *tree, struct kan_space_tree_ray_iterator_t *iterator)
{
    struct kan_space_tree_node_t *parent_node = NULL;
    while (KAN_TRUE)
    {
        // At the start of cycle iteration, there is 3 possible situations:
        // 1. It is the first cycle iteration and we're continuing tree iterator iteration.
        //    In this case, iterator->current_node is not null, so we're expected to go further along the ray.
        // 2. It is the first cycle iteration and we're starting from scratch.
        //    In this case, iterator->current_node is null, so we're getting the root and moving from root to current.
        // 3. It is second or further cycle iteration, needed because we've stumbled at zero child while following
        //    the current tree path. In this case iterator->current_node is always null and parent_node is never null.
        //    So, we need to go further in order to try to visit other nodes from hierarchy.
        kan_bool_t go_further = KAN_TRUE;

        if (iterator->current_node)
        {
            parent_node = kan_space_tree_node_get_parent (iterator->current_node);
            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->current_node->height == 0u)
                KAN_ASSERT (iterator->current_node == &tree->root)
                iterator->current_node = NULL;
                ray_calculate_previous_path_on_level (tree, iterator);
                return;
            }
        }

        if (!parent_node)
        {
            parent_node = &tree->root;
            go_further = KAN_FALSE;
        }

        if (go_further)
        {
            const kan_space_tree_road_t height_mask = node_height_mask (parent_node);
            const kan_space_tree_road_t height_to_root_mask = height_mask_to_root_to_height_mask (height_mask);

            if (iterator->current_path.combined == iterator->next_path.combined)
            {
                if (iterator->travelled_time >= iterator->max_time)
                {
                    // We've checked full ray time, now we just need to walk through parents up to the root.
                    iterator->current_node = parent_node;
                    ray_calculate_previous_path_on_level (tree, iterator);
                    return;
                }

                struct ray_target_and_dimension_t smallest =
                    calculate_ray_smallest_next (tree, iterator, height_mask, height_to_root_mask);

                if (smallest.out_of_bounds)
                {
                    // Gone out of bounds, therefore ray travel has ended.
                    iterator->travelled_time = iterator->max_time;
                    continue;
                }

                iterator->travelled_time += smallest.time;
                iterator->next_path.roads[smallest.dimension] = smallest.target;

                switch (tree->dimension_count)
                {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        iterator->position[DIMENSION] += iterator->direction[DIMENSION] * smallest.time;                               \
        if (DIMENSION != smallest.dimension)                                                                           \
        {                                                                                                              \
            iterator->next_path.roads[DIMENSION] = (kan_space_tree_road_t) iterator->position[DIMENSION];              \
        }

                    CASE (3u)
                    CASE (2u)
                    CASE (1u)
                    CASE (0u)
#undef CASE
                }
            }
        }

        const kan_space_tree_road_t height_mask = node_height_mask (parent_node);
        const kan_space_tree_road_t root_to_height_mask = height_mask_to_root_to_height_mask (height_mask);
        const kan_space_tree_road_t root_to_before_height_mask = root_to_height_mask ^ height_mask;

        kan_bool_t next_is_not_child = KAN_FALSE;
        switch (tree->dimension_count)
        {
        case 4u:
            next_is_not_child |= (iterator->next_path.roads[3u] & root_to_before_height_mask) !=
                                 (iterator->current_path.roads[3u] & root_to_before_height_mask);
        case 3u:
            next_is_not_child |= (iterator->next_path.roads[2u] & root_to_before_height_mask) !=
                                 (iterator->current_path.roads[2u] & root_to_before_height_mask);
        case 2u:
            next_is_not_child |= (iterator->next_path.roads[1u] & root_to_before_height_mask) !=
                                 (iterator->current_path.roads[1u] & root_to_before_height_mask);
        case 1u:
            next_is_not_child |= (iterator->next_path.roads[0u] & root_to_before_height_mask) !=
                                 (iterator->current_path.roads[0u] & root_to_before_height_mask);
        }

        if (next_is_not_child)
        {
            // Next is not child of current parent node, therefore we'll visit parent and continue upper.
            iterator->current_node = parent_node;
            ray_calculate_previous_path_on_level (tree, iterator);
            return;
        }

        // Next is a child, therefore we can finally descend to it.
        iterator->current_path = iterator->next_path;
        uint8_t child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);

        while (parent_node->children_allocation)
        {
            struct kan_space_tree_node_t *child_node = &parent_node->children_allocation->children[child_node_index];
            if (child_node->height == tree->last_level_height)
            {
                // Last level -- no children possible.
                iterator->current_node = child_node;
                ray_calculate_previous_path_on_level (tree, iterator);
                return;
            }

            parent_node = child_node;
            child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
        }

        // We've technically reached null node and will be repositioned in next while iteration.
        iterator->current_node = NULL;
    }
}

void kan_space_tree_init (struct kan_space_tree_t *tree,
                          kan_allocation_group_t allocation_group,
                          kan_instance_size_t dimension_count,
                          kan_instance_size_t sub_node_size,
                          kan_instance_size_t sub_node_alignment,
                          kan_space_tree_floating_t global_min,
                          kan_space_tree_floating_t global_max,
                          kan_space_tree_floating_t target_leaf_cell_size)
{
    KAN_ASSERT (global_max > global_min)
    KAN_ASSERT (sub_node_size < UINT16_MAX)
    KAN_ASSERT (sub_node_alignment < UINT16_MAX)

    tree->nodes_allocation_group = kan_allocation_group_get_child (allocation_group, "space_tree_nodes");
    tree->sub_nodes_allocation_group = kan_allocation_group_get_child (allocation_group, "space_tree_sub_nodes");

    KAN_ASSERT (dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
    tree->dimension_count = (uint8_t) dimension_count;
    tree->sub_node_size = (uint16_t) sub_node_size;
    tree->sub_node_alignment = (uint16_t) sub_node_alignment;
    tree->global_min = global_min;
    tree->global_max = global_max;
    tree->last_level_height = 1u;

    const kan_space_tree_floating_t half_width = 0.5 * (global_max - global_min);
    kan_space_tree_floating_t root_child_size = target_leaf_cell_size;

    while (root_child_size < half_width && tree->last_level_height < KAN_SPACE_TREE_MAX_HEIGHT)
    {
        root_child_size *= 2.0;
        ++tree->last_level_height;
    }

    tree->root.height = 0u;
    tree->root.sub_nodes_capacity = 0u;
    tree->root.sub_nodes_count = 0u;
    tree->root.sub_nodes = NULL;
    tree->root.children_allocation = NULL;
}

static inline void shape_iterator_init (struct kan_space_tree_t *tree,
                                        struct kan_space_tree_shape_iterator_t *iterator,
                                        const kan_space_tree_floating_t *min_sequence,
                                        const kan_space_tree_floating_t *max_sequence)
{
    iterator->current_node = NULL;
    iterator->min_path = quantize_sequence (tree, min_sequence);
    iterator->max_path = quantize_sequence (tree, max_sequence);
    iterator->current_path = iterator->min_path;
}

static inline uint8_t calculate_insertion_target_height (struct kan_space_tree_t *tree,
                                                         const kan_space_tree_floating_t *min_sequence,
                                                         const kan_space_tree_floating_t *max_sequence)
{
    kan_space_tree_floating_t average_dimension_size = 0.0;
    switch (tree->dimension_count)
    {
    case 4u:
        average_dimension_size += (max_sequence[3u] - min_sequence[3u]) / (kan_floating_t) tree->dimension_count;
    case 3u:
        average_dimension_size += (max_sequence[2u] - min_sequence[2u]) / (kan_floating_t) tree->dimension_count;
    case 2u:
        average_dimension_size += (max_sequence[1u] - min_sequence[1u]) / (kan_floating_t) tree->dimension_count;
    case 1u:
        average_dimension_size += (max_sequence[0u] - min_sequence[0u]) / (kan_floating_t) tree->dimension_count;
    }

    kan_space_tree_floating_t child_node_size = 0.125 * (tree->global_max - tree->global_min);
    uint8_t target_height = 1u;

    while (average_dimension_size < child_node_size && target_height < tree->last_level_height)
    {
        ++target_height;
        child_node_size *= 0.5;
    }

    return target_height;
}

struct kan_space_tree_insertion_iterator_t kan_space_tree_insertion_start (
    struct kan_space_tree_t *tree,
    const kan_space_tree_floating_t *min_sequence,
    const kan_space_tree_floating_t *max_sequence)
{
    struct kan_space_tree_insertion_iterator_t iterator;
    shape_iterator_init (tree, &iterator.base, min_sequence, max_sequence);
    iterator.target_height = calculate_insertion_target_height (tree, min_sequence, max_sequence);
    insertion_iterator_next (tree, &iterator);
    return iterator;
}

static inline void kan_space_tree_node_reallocate_sub_nodes (struct kan_space_tree_t *tree,
                                                             struct kan_space_tree_node_t *node,
                                                             uint16_t new_capacity)
{
    const uint16_t old_capacity = node->sub_nodes_capacity;
    void *old_sub_nodes = node->sub_nodes;

    node->sub_nodes_capacity = new_capacity;
    node->sub_nodes = kan_allocate_general (tree->sub_nodes_allocation_group, tree->sub_node_size * new_capacity,
                                            tree->sub_node_alignment);

    if (old_sub_nodes)
    {
        memcpy (node->sub_nodes, old_sub_nodes, tree->sub_node_size * node->sub_nodes_count);
        kan_free_general (tree->sub_nodes_allocation_group, old_sub_nodes, tree->sub_node_size * old_capacity);
    }
}

void *kan_space_tree_insertion_insert_and_move (struct kan_space_tree_t *tree,
                                                struct kan_space_tree_insertion_iterator_t *iterator)
{
    KAN_ASSERT (!kan_space_tree_insertion_is_finished (iterator))
    struct kan_space_tree_node_t *node = iterator->base.current_node;

    if (node->sub_nodes_count >= node->sub_nodes_capacity)
    {
        kan_space_tree_node_reallocate_sub_nodes (tree, node,
                                                  node->sub_nodes_capacity + KAN_CONTAINER_SPACE_TREE_SUB_NODE_SLICE);
    }

    void *new_sub_node = ((uint8_t *) node->sub_nodes) + tree->sub_node_size * node->sub_nodes_count;
    ++node->sub_nodes_count;

    insertion_iterator_next (tree, iterator);
    return new_sub_node;
}

CONTAINER_API struct kan_space_tree_shape_iterator_t kan_space_tree_shape_start (
    struct kan_space_tree_t *tree,
    const kan_space_tree_floating_t *min_sequence,
    const kan_space_tree_floating_t *max_sequence)
{
    struct kan_space_tree_shape_iterator_t iterator;
    shape_iterator_init (tree, &iterator, min_sequence, max_sequence);
    shape_iterator_next (tree, &iterator);
    return iterator;
}

CONTAINER_API void kan_space_tree_shape_move_to_next_node (struct kan_space_tree_t *tree,
                                                           struct kan_space_tree_shape_iterator_t *iterator)
{
    shape_iterator_next (tree, iterator);
}

kan_bool_t kan_space_tree_shape_is_first_occurrence (struct kan_space_tree_t *tree,
                                                     struct kan_space_tree_quantized_path_t object_min,
                                                     struct kan_space_tree_shape_iterator_t *iterator)
{
    if (iterator->current_node->height == 0u)
    {
        return KAN_TRUE;
    }

    const kan_space_tree_road_t mask =
        height_mask_to_root_to_height_mask (make_height_mask (iterator->current_node->height - 1u));

    switch (tree->dimension_count)
    {
    case 4u:
        if (KAN_MAX (object_min.roads[3u] & mask, iterator->min_path.roads[3u] & mask) !=
            (iterator->current_path.roads[3u] & mask))
        {
            return KAN_FALSE;
        }
    case 3u:
        if (KAN_MAX (object_min.roads[2u] & mask, iterator->min_path.roads[2u] & mask) !=
            (iterator->current_path.roads[2u] & mask))
        {
            return KAN_FALSE;
        }
    case 2u:
        if (KAN_MAX (object_min.roads[1u] & mask, iterator->min_path.roads[1u] & mask) !=
            (iterator->current_path.roads[1u] & mask))
        {
            return KAN_FALSE;
        }
    case 1u:
        if (KAN_MAX (object_min.roads[0u] & mask, iterator->min_path.roads[0u] & mask) !=
            (iterator->current_path.roads[0u] & mask))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

struct kan_space_tree_ray_iterator_t kan_space_tree_ray_start (struct kan_space_tree_t *tree,
                                                               const kan_space_tree_floating_t *origin_sequence,
                                                               const kan_space_tree_floating_t *direction_sequence,
                                                               kan_space_tree_floating_t max_time)
{
    struct kan_space_tree_ray_iterator_t iterator;
    iterator.current_path = quantize_sequence (tree, origin_sequence);
    iterator.next_path = iterator.current_path;
    iterator.has_previous_path_on_level = KAN_FALSE;
    iterator.previous_path_on_level = iterator.current_path;
    iterator.current_node = NULL;

#if defined(KAN_WITH_ASSERT)
    switch (tree->dimension_count)
    {
    case 4u:
        KAN_ASSERT (direction_sequence[0u] != 0.0 || direction_sequence[1u] != 0.0 || direction_sequence[2u] != 0.0 ||
                    direction_sequence[3u] != 0.0)
        break;
    case 3u:
        KAN_ASSERT (direction_sequence[0u] != 0.0 || direction_sequence[1u] != 0.0 || direction_sequence[2u] != 0.0)
        break;
    case 2u:
        KAN_ASSERT (direction_sequence[0u] != 0.0 || direction_sequence[1u] != 0.0)
        break;
    case 1u:
        KAN_ASSERT (direction_sequence[0u] != 0.0)
        break;
    }
#endif

    const kan_space_tree_floating_t factor =
        ((kan_space_tree_floating_t) KAN_INT_MAX (kan_space_tree_road_t)) / (tree->global_max - tree->global_min);
    switch (tree->dimension_count)
    {
    case 4u:
        iterator.position[3u] = to_quantized_space (origin_sequence[3u], tree->global_min, tree->global_max);
        iterator.direction[3u] = factor * direction_sequence[3u];
    case 3u:
        iterator.position[2u] = to_quantized_space (origin_sequence[2u], tree->global_min, tree->global_max);
        iterator.direction[2u] = factor * direction_sequence[2u];
    case 2u:
        iterator.position[1u] = to_quantized_space (origin_sequence[1u], tree->global_min, tree->global_max);
        iterator.direction[1u] = factor * direction_sequence[1u];
    case 1u:
        iterator.position[0u] = to_quantized_space (origin_sequence[0u], tree->global_min, tree->global_max);
        iterator.direction[0u] = factor * direction_sequence[0u];
    }

    iterator.travelled_time = (kan_floating_t) 0.0;
    KAN_ASSERT (max_time > 0.0)
    iterator.max_time = max_time;

    ray_iterator_next (tree, &iterator);
    return iterator;
}

void kan_space_tree_ray_move_to_next_node (struct kan_space_tree_t *tree,
                                           struct kan_space_tree_ray_iterator_t *iterator)
{
    ray_iterator_next (tree, iterator);
}

kan_bool_t kan_space_tree_ray_is_first_occurrence (struct kan_space_tree_t *tree,
                                                   struct kan_space_tree_quantized_path_t object_min,
                                                   struct kan_space_tree_quantized_path_t object_max,
                                                   struct kan_space_tree_ray_iterator_t *iterator)
{
    if (!iterator->has_previous_path_on_level)
    {
        // There was nothing before, so it is always a first occurrence.
        return KAN_TRUE;
    }

    if (object_min.combined == object_max.combined)
    {
        // Object is stored inside one node, therefore we can safely say it is unique.
        return KAN_TRUE;
    }

    const kan_space_tree_road_t mask =
        height_mask_to_root_to_height_mask (make_height_mask (iterator->current_node->height - 1u));

    // Sub node is the first occurrence for the ray only and if only
    // previous path on this level didn't contain the same object.
    switch (tree->dimension_count)
    {
    case 4u:
        if ((iterator->previous_path_on_level.roads[3u] & mask) < (object_min.roads[3u] & mask) ||
            (iterator->previous_path_on_level.roads[3u] & mask) > (object_max.roads[3u] & mask))
        {
            return KAN_TRUE;
        }
    case 3u:
        if ((iterator->previous_path_on_level.roads[2u] & mask) < (object_min.roads[2u] & mask) ||
            (iterator->previous_path_on_level.roads[2u] & mask) > (object_max.roads[2u] & mask))
        {
            return KAN_TRUE;
        }
    case 2u:
        if ((iterator->previous_path_on_level.roads[1u] & mask) < (object_min.roads[1u] & mask) ||
            (iterator->previous_path_on_level.roads[1u] & mask) > (object_max.roads[1u] & mask))
        {
            return KAN_TRUE;
        }
    case 1u:
        if ((iterator->previous_path_on_level.roads[0u] & mask) < (object_min.roads[0u] & mask) ||
            (iterator->previous_path_on_level.roads[0u] & mask) > (object_max.roads[0u] & mask))
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

kan_bool_t kan_space_tree_is_re_insert_needed (struct kan_space_tree_t *tree,
                                               const kan_space_tree_floating_t *old_min,
                                               const kan_space_tree_floating_t *old_max,
                                               const kan_space_tree_floating_t *new_min,
                                               const kan_space_tree_floating_t *new_max)
{
    const kan_space_tree_road_t old_height = calculate_insertion_target_height (tree, old_min, old_max);
    const kan_space_tree_road_t new_height = calculate_insertion_target_height (tree, new_min, new_max);

    if (old_height != new_height)
    {
        return KAN_TRUE;
    }

    const struct kan_space_tree_quantized_path_t old_min_path = quantize_sequence (tree, old_min);
    const struct kan_space_tree_quantized_path_t old_max_path = quantize_sequence (tree, old_max);

    const struct kan_space_tree_quantized_path_t new_min_path = quantize_sequence (tree, new_min);
    const struct kan_space_tree_quantized_path_t new_max_path = quantize_sequence (tree, new_max);
    const kan_space_tree_road_t height_mask = make_height_mask (old_height);

    switch (tree->dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if ((old_min_path.roads[DIMENSION] & height_mask) != (new_min_path.roads[DIMENSION] & height_mask))            \
        {                                                                                                              \
            return KAN_TRUE;                                                                                           \
        }                                                                                                              \
                                                                                                                       \
        if ((old_max_path.roads[DIMENSION] & height_mask) != (new_max_path.roads[DIMENSION] & height_mask))            \
        {                                                                                                              \
            return KAN_TRUE;                                                                                           \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    return KAN_FALSE;
}

kan_bool_t kan_space_tree_is_contained_in_one_sub_node (struct kan_space_tree_t *tree,
                                                        const kan_space_tree_floating_t *min,
                                                        const kan_space_tree_floating_t *max)
{
    const kan_space_tree_road_t height = calculate_insertion_target_height (tree, min, max);
    const struct kan_space_tree_quantized_path_t min_path = quantize_sequence (tree, min);
    const struct kan_space_tree_quantized_path_t max_path = quantize_sequence (tree, max);
    const kan_space_tree_road_t height_mask = make_height_mask (height);
    const kan_space_tree_road_t root_to_before_height_mask =
        height_mask_to_root_to_height_mask (height_mask) ^ height_mask;

    switch (tree->dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
        if ((min_path.roads[DIMENSION] & root_to_before_height_mask) !=                                                \
            (max_path.roads[DIMENSION] & root_to_before_height_mask))                                                  \
        {                                                                                                              \
            return KAN_FALSE;                                                                                          \
        }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    return KAN_TRUE;
}

static kan_bool_t is_node_empty (struct kan_space_tree_t *tree, struct kan_space_tree_node_t *node)
{
    if (node->sub_nodes_count > 0u)
    {
        return KAN_FALSE;
    }

    if (node->children_allocation)
    {
        const kan_instance_size_t children_count = 1u << tree->dimension_count;
        for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) children_count; ++index)
        {
            struct kan_space_tree_node_t *child_node = &node->children_allocation->children[index];
            // Intentionally do not use recursion here as we're only using this function to go from bottom to top.
            if (child_node->sub_nodes_count > 0u || child_node->children_allocation)
            {
                return KAN_FALSE;
            }
        }
    }

    // Either no children or all children are empty.
    return KAN_TRUE;
}

static inline void kan_space_tree_node_shutdown_empty (struct kan_space_tree_t *tree,
                                                       struct kan_space_tree_node_t *node)
{
    if (node->sub_nodes)
    {
        kan_free_general (tree->sub_nodes_allocation_group, node->sub_nodes,
                          tree->sub_node_size * node->sub_nodes_capacity);

        node->sub_nodes_capacity = 0u;
        node->sub_nodes_count = 0u;
        node->sub_nodes = NULL;
    }

    // We only use this function for empty nodes, therefore no need for recursive deallocation of children.
    if (node->children_allocation)
    {
        kan_free_batched (tree->nodes_allocation_group, node->children_allocation);
        node->children_allocation = NULL;
    }
}

void kan_space_tree_delete (struct kan_space_tree_t *tree, struct kan_space_tree_node_t *node, void *sub_node)
{
    KAN_ASSERT (sub_node >= node->sub_nodes &&
                (uint8_t *) sub_node < ((uint8_t *) node->sub_nodes) + tree->sub_node_size * node->sub_nodes_capacity)

    void *last_sub_node = ((uint8_t *) node->sub_nodes) + tree->sub_node_size * (node->sub_nodes_count - 1u);
    if (sub_node != last_sub_node)
    {
        memcpy (sub_node, last_sub_node, tree->sub_node_size);
    }

    --node->sub_nodes_count;
    if (node->sub_nodes_count > 0u)
    {
        // Do not bother with empty nodes as we'll just delete them.
        if (node->sub_nodes_capacity - node->sub_nodes_count >= (uint16_t) 2u * KAN_CONTAINER_SPACE_TREE_SUB_NODE_SLICE)
        {
            kan_space_tree_node_reallocate_sub_nodes (
                tree, node, node->sub_nodes_capacity - KAN_CONTAINER_SPACE_TREE_SUB_NODE_SLICE);
        }
    }

    while (node != &tree->root && is_node_empty (tree, node))
    {
        struct kan_space_tree_node_t *parent = kan_space_tree_node_get_parent (node);
        kan_space_tree_node_shutdown_empty (tree, node);
        node = parent;
    }
}

static void space_tree_destroy_node (struct kan_space_tree_t *tree, struct kan_space_tree_node_t *node)
{
    if (!node)
    {
        return;
    }

    if (node->children_allocation)
    {
        const kan_instance_size_t children_count = 1u << tree->dimension_count;
        for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) children_count; ++index)
        {
            space_tree_destroy_node (tree, &node->children_allocation->children[index]);
        }
    }

    kan_space_tree_node_shutdown_empty (tree, node);
}

void kan_space_tree_shutdown (struct kan_space_tree_t *tree)
{
    space_tree_destroy_node (tree, &tree->root);
}
