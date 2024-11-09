#include <float.h>
#include <stddef.h>

#include <kan/api_common/min_max.h>
#include <kan/container/space_tree.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

// TODO: Current implementation is very good at keeping memory usage low, but it's not efficient due to cache misses.
//       We use 3.5 less memory than UE5 Octree, but we're also 3-10 times slower than UE5 octree.
//       This needs to be investigated and improved. The best tool for doing it is `perf` on Linux.

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

static inline kan_space_tree_road_t calculate_child_node_index (struct kan_space_tree_t *tree,
                                                                struct kan_space_tree_node_t *node,
                                                                struct kan_space_tree_quantized_path_t path)
{
    kan_space_tree_road_t index = 0u;
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
            parent_node = iterator->current_node->parent;

            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->current_node->height == 0u)
                KAN_ASSERT (iterator->current_node == tree->root)
                iterator->current_node = NULL;
                return;
            }
        }

        if (!parent_node)
        {
            // We don't have current, it means that we're either starting from scratch or restarting iteration again.
            parent_node = tree->root;
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
                return;
            }
        }

        kan_space_tree_road_t child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
        struct kan_space_tree_node_t *child_node = parent_node->children[child_node_index];

        while (child_node)
        {
            if (child_node->height == tree->last_level_height)
            {
                // Last level -- no children possible.
                iterator->current_node = child_node;
                return;
            }

            parent_node = child_node;
            const kan_space_tree_road_t child_height_mask = node_height_mask (child_node);
            const kan_space_tree_road_t reversed_child_height_mask = ~child_height_mask;

            shape_iterator_reset_all_dimensions (tree, iterator, height_mask_to_root_to_height_mask (child_height_mask),
                                                 child_height_mask, reversed_child_height_mask);
            child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
            child_node = parent_node->children[child_node_index];
        }

        // We've technically reached null node and will be repositioned in next while iteration.
        iterator->current_node = NULL;
    }
}

static inline kan_instance_size_t calculate_node_size (uint8_t dimension_count, kan_bool_t with_children)
{
    kan_instance_size_t size = sizeof (struct kan_space_tree_node_t);
    if (with_children)
    {
        size += sizeof (void *) * (kan_instance_size_t) (1u << (kan_instance_size_t) dimension_count);
    }

    return kan_apply_alignment (size, _Alignof (struct kan_space_tree_node_t));
}

static inline void reset_node_children (struct kan_space_tree_t *tree, struct kan_space_tree_node_t *node)
{
    switch (tree->dimension_count)
    {
    case 4u:
        node->children[15u] = NULL;
        node->children[14u] = NULL;
        node->children[13u] = NULL;
        node->children[12u] = NULL;
        node->children[11u] = NULL;
        node->children[10u] = NULL;
        node->children[9u] = NULL;
        node->children[8u] = NULL;
    case 3u:
        node->children[7u] = NULL;
        node->children[6u] = NULL;
        node->children[5u] = NULL;
        node->children[4u] = NULL;
    case 2u:
        node->children[3u] = NULL;
        node->children[2u] = NULL;
    case 1u:
        node->children[1u] = NULL;
        node->children[0u] = NULL;
    }
}

static inline struct kan_space_tree_node_t *get_or_create_child_node (struct kan_space_tree_t *tree,
                                                                      struct kan_space_tree_node_t *parent_node,
                                                                      kan_space_tree_road_t child_index)
{
    KAN_ASSERT (parent_node->height != tree->last_level_height)
    if (parent_node->children[child_index])
    {
        return parent_node->children[child_index];
    }

    const kan_space_tree_road_t child_height = parent_node->height + 1u;
    struct kan_space_tree_node_t *child_node = kan_allocate_batched (
        tree->allocation_group, calculate_node_size (tree->dimension_count, child_height != tree->last_level_height));

    child_node->parent = parent_node;
    child_node->height = (uint8_t) child_height;
    child_node->first_sub_node = NULL;

    if (child_height != tree->last_level_height)
    {
        reset_node_children (tree, child_node);
    }

    parent_node->children[child_index] = child_node;
    return child_node;
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
            parent_node = iterator->base.current_node->parent;

            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->base.current_node->height == 0u)
                KAN_ASSERT (iterator->base.current_node == tree->root)
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
            parent_node = tree->root;
        }

        kan_space_tree_road_t child_node_index =
            calculate_child_node_index (tree, parent_node, iterator->base.current_path);
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

struct ray_next_t
{
    kan_space_tree_road_t next;
    kan_space_tree_floating_t time;
    kan_bool_t out_of_bounds;
};

static inline struct ray_next_t get_ray_next_in_dimension (struct kan_space_tree_ray_iterator_t *iterator,
                                                           kan_space_tree_road_t dimension_index,
                                                           kan_space_tree_road_t height_mask,
                                                           kan_space_tree_road_t height_to_root_mask)
{
    struct ray_next_t result;
    kan_space_tree_floating_t border_value;

    if (iterator->direction[dimension_index] > (kan_floating_t) 0.0)
    {
        const kan_space_tree_road_t masked_current =
            iterator->current_path.roads[dimension_index] & height_to_root_mask;
        result.next = masked_current + height_mask;

        if (result.next < masked_current)
        {
            // Overflow.
            border_value = (kan_space_tree_floating_t) KAN_INT_MAX (kan_space_tree_road_t);
            result.out_of_bounds = KAN_TRUE;
        }
        else
        {
            border_value = (kan_space_tree_floating_t) result.next;
            result.out_of_bounds = KAN_FALSE;
        }
    }
    else if (iterator->direction[dimension_index] < (kan_floating_t) 0.0)
    {
        const kan_space_tree_road_t masked_current =
            iterator->current_path.roads[dimension_index] | ~height_to_root_mask;
        result.next = masked_current - height_mask;

        if (result.next > masked_current)
        {
            // Underflow.
            border_value = (kan_floating_t) 0.0;
            result.out_of_bounds = KAN_TRUE;
        }
        else
        {
            border_value = (kan_space_tree_floating_t) result.next;
            result.out_of_bounds = KAN_FALSE;
        }
    }
    else
    {
        result.time = DBL_MAX;
        result.out_of_bounds = KAN_TRUE;
        return result;
    }

    const kan_space_tree_floating_t distance_to_border = border_value - iterator->position[dimension_index];
    result.time = distance_to_border / iterator->direction[dimension_index];
    KAN_ASSERT (result.time >= 0.0)
    return result;
}

struct ray_next_and_dimension_t
{
    kan_space_tree_road_t dimension;
    kan_space_tree_road_t next;
    kan_space_tree_floating_t time;
    kan_bool_t out_of_bounds;
};

static inline struct ray_next_and_dimension_t calculate_ray_smallest_next (
    struct kan_space_tree_t *tree,
    struct kan_space_tree_ray_iterator_t *iterator,
    kan_space_tree_road_t height_mask,
    kan_space_tree_road_t height_to_root_mask)
{
    struct ray_next_and_dimension_t min = {KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS + 1u, 0u, DBL_MAX, KAN_TRUE};
    switch (tree->dimension_count)
    {
#define CASE(DIMENSION)                                                                                                \
    case (DIMENSION + 1u):                                                                                             \
    {                                                                                                                  \
        struct ray_next_t result = get_ray_next_in_dimension (iterator, DIMENSION, height_mask, height_to_root_mask);  \
        if (result.time < min.time)                                                                                    \
        {                                                                                                              \
            min.dimension = DIMENSION;                                                                                 \
            min.next = result.next;                                                                                    \
            min.time = result.time;                                                                                    \
            min.out_of_bounds = result.out_of_bounds;                                                                  \
        }                                                                                                              \
    }

        CASE (3u)
        CASE (2u)
        CASE (1u)
        CASE (0u)
#undef CASE
    }

    return min;
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
            parent_node = iterator->current_node->parent;
            if (!parent_node)
            {
                // We've visited root, that means we've already visited everything.
                KAN_ASSERT (iterator->current_node->height == 0u)
                KAN_ASSERT (iterator->current_node == tree->root)
                iterator->current_node = NULL;
                return;
            }
        }

        if (!parent_node)
        {
            parent_node = tree->root;
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
                    return;
                }

                struct ray_next_and_dimension_t smallest =
                    calculate_ray_smallest_next (tree, iterator, height_mask, height_to_root_mask);

                if (smallest.out_of_bounds)
                {
                    // Gone out of bounds, therefore ray travel has ended.
                    iterator->travelled_time = iterator->max_time;
                    continue;
                }

                iterator->travelled_time += smallest.time;
                iterator->next_path.roads[smallest.dimension] = smallest.next;

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
            return;
        }

        // Next is a child, therefore we can finally descend to it.
        iterator->current_path = iterator->next_path;
        kan_space_tree_road_t child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
        struct kan_space_tree_node_t *child_node = parent_node->children[child_node_index];

        while (child_node)
        {
            if (child_node->height == tree->last_level_height)
            {
                // Last level -- no children possible.
                iterator->current_node = child_node;
                return;
            }

            parent_node = child_node;
            child_node_index = calculate_child_node_index (tree, parent_node, iterator->current_path);
            child_node = parent_node->children[child_node_index];
        }

        // We've technically reached null node and will be repositioned in next while iteration.
        iterator->current_node = NULL;
    }
}

void kan_space_tree_init (struct kan_space_tree_t *tree,
                          kan_allocation_group_t allocation_group,
                          kan_instance_size_t dimension_count,
                          kan_space_tree_floating_t global_min,
                          kan_space_tree_floating_t global_max,
                          kan_space_tree_floating_t target_leaf_cell_size)
{
    KAN_ASSERT (global_max > global_min)

    tree->allocation_group = allocation_group;
    KAN_ASSERT (dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
    tree->dimension_count = (kan_space_tree_road_t) dimension_count;
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

    tree->root = (struct kan_space_tree_node_t *) kan_allocate_batched (
        allocation_group, calculate_node_size (tree->dimension_count, KAN_TRUE));
    tree->root->parent = NULL;
    tree->root->height = 0u;
    tree->root->first_sub_node = NULL;
    reset_node_children (tree, tree->root);
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

static inline kan_space_tree_road_t calculate_insertion_target_height (struct kan_space_tree_t *tree,
                                                                       const kan_space_tree_floating_t *min_sequence,
                                                                       const kan_space_tree_floating_t *max_sequence)
{
    kan_space_tree_floating_t max_dimension_size = 0.0;
    switch (tree->dimension_count)
    {
    case 4u:
        max_dimension_size = KAN_MAX (max_dimension_size, max_sequence[3u] - min_sequence[3u]);
    case 3u:
        max_dimension_size = KAN_MAX (max_dimension_size, max_sequence[2u] - min_sequence[2u]);
    case 2u:
        max_dimension_size = KAN_MAX (max_dimension_size, max_sequence[1u] - min_sequence[1u]);
    case 1u:
        max_dimension_size = KAN_MAX (max_dimension_size, max_sequence[0u] - min_sequence[0u]);
    }

    kan_space_tree_floating_t child_node_size = 0.125 * (tree->global_max - tree->global_min);
    kan_space_tree_road_t target_height = 1u;

    while (max_dimension_size < child_node_size && target_height < tree->last_level_height)
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

void kan_space_tree_insertion_insert_and_move (struct kan_space_tree_t *tree,
                                               struct kan_space_tree_insertion_iterator_t *iterator,
                                               struct kan_space_tree_sub_node_t *sub_node)
{
    KAN_ASSERT (!kan_space_tree_insertion_is_finished (iterator))
    struct kan_space_tree_node_t *node = iterator->base.current_node;

    sub_node->next = node->first_sub_node;
    sub_node->previous = NULL;

    if (node->first_sub_node)
    {
        node->first_sub_node->previous = sub_node;
    }

    node->first_sub_node = sub_node;
    insertion_iterator_next (tree, iterator);
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

struct kan_space_tree_ray_iterator_t kan_space_tree_ray_start (struct kan_space_tree_t *tree,
                                                               const kan_space_tree_floating_t *origin_sequence,
                                                               const kan_space_tree_floating_t *direction_sequence,
                                                               kan_space_tree_floating_t max_time)
{
    struct kan_space_tree_ray_iterator_t iterator;
    iterator.current_path = quantize_sequence (tree, origin_sequence);
    iterator.next_path = iterator.current_path;
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
    if (node->first_sub_node)
    {
        return KAN_FALSE;
    }

    kan_bool_t has_children = KAN_FALSE;
    if (node->height != tree->last_level_height)
    {
        switch (tree->dimension_count)
        {
        case 4u:
            has_children = node->children[0u] || node->children[1u] || node->children[2u] || node->children[3u] ||
                           node->children[4u] || node->children[5u] || node->children[6u] || node->children[7u] ||
                           node->children[8u] || node->children[9u] || node->children[10u] || node->children[11u] ||
                           node->children[12u] || node->children[13u] || node->children[14u] || node->children[15u];
            break;
        case 3u:
            has_children = node->children[0u] || node->children[1u] || node->children[2u] || node->children[3u] ||
                           node->children[4u] || node->children[5u] || node->children[6u] || node->children[7u];
            break;
        case 2u:
            has_children = node->children[0u] || node->children[1u] || node->children[2u] || node->children[3u];
            break;
        case 1u:
            has_children = node->children[0u] || node->children[1u];
            break;
        }
    }

    return !has_children;
}

void kan_space_tree_delete (struct kan_space_tree_t *tree,
                            struct kan_space_tree_node_t *node,
                            struct kan_space_tree_sub_node_t *sub_node)
{
    if (sub_node->previous)
    {
        sub_node->previous->next = sub_node->next;
    }
    else
    {
        KAN_ASSERT (node->first_sub_node == sub_node)
        node->first_sub_node = sub_node->next;
    }

    if (sub_node->next)
    {
        sub_node->next->previous = sub_node->previous;
    }

    while (node != tree->root && is_node_empty (tree, node))
    {
        struct kan_space_tree_node_t *parent = node->parent;
        kan_free_batched (tree->allocation_group, node);

        // Could be optimized out if we knew node path.
        switch (tree->dimension_count)
        {
#define IF(INDEX)                                                                                                      \
    if (parent->children[INDEX] == node)                                                                               \
    {                                                                                                                  \
        parent->children[INDEX] = NULL;                                                                                \
        break;                                                                                                         \
    }

        case 4u:
            IF (15u)
            IF (14u)
            IF (13u)
            IF (12u)
            IF (11u)
            IF (10u)
            IF (9u)
            IF (8u)
        case 3u:
            IF (7u)
            IF (6u)
            IF (5u)
            IF (4u)
        case 2u:
            IF (3u)
            IF (2u)
        case 1u:
            IF (1u)
            IF (0u)
#undef IF
        }

        node = parent;
    }
}

static void space_tree_destroy_node (struct kan_space_tree_t *tree, struct kan_space_tree_node_t *node)
{
    if (!node)
    {
        return;
    }

    if (node->height != tree->last_level_height)
    {
        switch (tree->dimension_count)
        {
        case 4u:
            space_tree_destroy_node (tree, node->children[15u]);
            space_tree_destroy_node (tree, node->children[14u]);
            space_tree_destroy_node (tree, node->children[13u]);
            space_tree_destroy_node (tree, node->children[12u]);
            space_tree_destroy_node (tree, node->children[11u]);
            space_tree_destroy_node (tree, node->children[10u]);
            space_tree_destroy_node (tree, node->children[9u]);
            space_tree_destroy_node (tree, node->children[8u]);
        case 3u:
            space_tree_destroy_node (tree, node->children[7u]);
            space_tree_destroy_node (tree, node->children[6u]);
            space_tree_destroy_node (tree, node->children[5u]);
            space_tree_destroy_node (tree, node->children[4u]);
        case 2u:
            space_tree_destroy_node (tree, node->children[3u]);
            space_tree_destroy_node (tree, node->children[2u]);
        case 1u:
            space_tree_destroy_node (tree, node->children[1u]);
            space_tree_destroy_node (tree, node->children[0u]);
        }
    }

    kan_free_batched (tree->allocation_group, node);
}

void kan_space_tree_shutdown (struct kan_space_tree_t *tree)
{
    space_tree_destroy_node (tree, tree->root);
}
