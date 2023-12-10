#include <stddef.h>

#include <kan/container/avl_tree.h>
#include <kan/error/critical.h>

static inline struct kan_avl_tree_node_t *left_rotate (struct kan_avl_tree_t *tree, struct kan_avl_tree_node_t *node)
{
    struct kan_avl_tree_node_t *right = node->right;
    KAN_ASSERT (right)
    struct kan_avl_tree_node_t *right_left = right->left;
    node->right = right_left;

    if (right_left)
    {
        right_left->parent = node;
    }

    right->parent = node->parent;
    if (!right->parent)
    {
        KAN_ASSERT (tree->root == node)
        tree->root = right;
    }
    else
    {
        if (node == node->parent->left)
        {
            node->parent->left = right;
        }
        else
        {
            KAN_ASSERT (node == node->parent->right)
            node->parent->right = right;
        }
    }

    right->left = node;
    node->parent = right;
    return right;
}

static inline struct kan_avl_tree_node_t *right_rotate (struct kan_avl_tree_t *tree, struct kan_avl_tree_node_t *node)
{
    struct kan_avl_tree_node_t *left = node->left;
    KAN_ASSERT (left)
    struct kan_avl_tree_node_t *left_right = left->right;
    node->left = left_right;

    if (left_right)
    {
        left_right->parent = node;
    }

    left->parent = node->parent;
    if (!left->parent)
    {
        KAN_ASSERT (tree->root == node)
        tree->root = left;
    }
    else
    {
        if (node == node->parent->left)
        {
            node->parent->left = left;
        }
        else
        {
            KAN_ASSERT (node == node->parent->right)
            node->parent->right = left;
        }
    }

    left->right = node;
    node->parent = left;
    return left;
}

void kan_avl_tree_init (struct kan_avl_tree_t *tree)
{
    tree->root = NULL;
    tree->size = 0u;
}

struct kan_avl_tree_node_t *kan_avl_tree_find_parent_for_insertion (struct kan_avl_tree_t *tree, uint64_t tree_value)
{
    if (!tree->root)
    {
        return NULL;
    }

    struct kan_avl_tree_node_t *current = tree->root;
    while (KAN_TRUE)
    {
        if (tree_value < current->tree_value)
        {
            if (!current->left)
            {
                return current;
            }

            current = current->left;
        }
        else if (tree_value > current->tree_value)
        {
            if (!current->right)
            {
                return current;
            }

            current = current->right;
        }
        else
        {
            return current;
        }
    }
}

void kan_avl_tree_insert (struct kan_avl_tree_t *tree,
                          struct kan_avl_tree_node_t *found_parent,
                          struct kan_avl_tree_node_t *node)
{
    KAN_ASSERT (kan_avl_tree_can_insert (found_parent, node->tree_value))
    node->parent = found_parent;
    node->left = NULL;
    node->right = NULL;
    node->balance_factor = 0u;
    ++tree->size;

    if (!found_parent)
    {
        KAN_ASSERT (!tree->root)
        KAN_ASSERT (tree->size == 1u)
        tree->root = node;
        return;
    }

    if (node->tree_value < found_parent->tree_value)
    {
        found_parent->left = node;
    }
    else
    {
        found_parent->right = node;
    }

    struct kan_avl_tree_node_t *retrace_parent = found_parent;
    struct kan_avl_tree_node_t *retrace_child = node;

    while (retrace_parent)
    {
        if (retrace_child == retrace_parent->left)
        {
            switch (retrace_parent->balance_factor)
            {
            case -1:
                KAN_ASSERT (retrace_parent->left)
                if (retrace_parent->left->balance_factor == -1)
                {
                    retrace_parent = right_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;
                    retrace_parent->right->balance_factor = 0;
                }
                else
                {
                    KAN_ASSERT (retrace_parent->left->right)
                    const int8_t deep_balance_factor = retrace_parent->left->right->balance_factor;

                    left_rotate (tree, retrace_parent->left);
                    retrace_parent = right_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;

                    switch (deep_balance_factor)
                    {
                    case -1:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 1;
                        break;

                    case 0:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    case 1:
                        retrace_parent->left->balance_factor = -1;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    default:
                        KAN_ASSERT (KAN_FALSE)
                        break;
                    }
                }

                // Balanced, exit retrace.
                return;

            case 0:
                retrace_parent->balance_factor = -1;
                // Gone out of balance, we need to continue the retrace.
                break;

            case 1:
                retrace_parent->balance_factor = 0;
                // Balanced, exit retrace.
                return;

            default:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }
        else
        {
            KAN_ASSERT (retrace_child == retrace_parent->right)
            switch (retrace_parent->balance_factor)
            {
            case -1:
                retrace_parent->balance_factor = 0;
                // Balanced, exit retrace.
                return;

            case 0:
                retrace_parent->balance_factor = 1;
                // Gone out of balance, we need to continue the retrace.
                break;

            case 1:
                KAN_ASSERT (retrace_parent->right)
                if (retrace_parent->right->balance_factor == 1)
                {
                    retrace_parent = left_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;
                    retrace_parent->left->balance_factor = 0;
                }
                else
                {
                    KAN_ASSERT (retrace_parent->right->left)
                    const int8_t deep_balance_factor = retrace_parent->right->left->balance_factor;

                    right_rotate (tree, retrace_parent->right);
                    retrace_parent = left_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;

                    switch (deep_balance_factor)
                    {
                    case -1:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 1;
                        break;

                    case 0:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    case 1:
                        retrace_parent->left->balance_factor = -1;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    default:
                        KAN_ASSERT (KAN_FALSE)
                        break;
                    }
                }

                // Balanced, exit retrace.
                return;

            default:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }

        retrace_child = retrace_parent;
        retrace_parent = retrace_parent->parent;
    }
}

static struct kan_avl_tree_node_t *find_closest (struct kan_avl_tree_t *tree, uint64_t tree_value)
{
    struct kan_avl_tree_node_t *current = tree->root;
    while (current)
    {
        if (tree_value < current->tree_value)
        {
            if (!current->left)
            {
                break;
            }

            current = current->left;
        }
        else if (tree_value > current->tree_value)
        {
            if (!current->right)
            {
                break;
            }

            current = current->right;
        }
        else
        {
            break;
        }
    }

    return current;
}

struct kan_avl_tree_node_t *kan_avl_tree_find_equal (struct kan_avl_tree_t *tree, uint64_t tree_value)
{
    struct kan_avl_tree_node_t *closest = find_closest (tree, tree_value);
    return closest && closest->tree_value == tree_value ? closest : NULL;
}

struct kan_avl_tree_node_t *kan_avl_tree_find_upper_bound (struct kan_avl_tree_t *tree, uint64_t tree_value)
{
    struct kan_avl_tree_node_t *closest = find_closest (tree, tree_value);
    if (!closest)
    {
        return NULL;
    }

    if (closest->tree_value <= tree_value)
    {
        return kan_avl_tree_ascending_iteration_next (closest);
    }
    else
    {
        return closest;
    }
}

struct kan_avl_tree_node_t *kan_avl_tree_find_lower_bound (struct kan_avl_tree_t *tree, uint64_t tree_value)
{
    struct kan_avl_tree_node_t *closest = find_closest (tree, tree_value);
    if (!closest)
    {
        return NULL;
    }

    if (closest->tree_value >= tree_value)
    {
        return kan_avl_tree_descending_iteration_next (closest);
    }
    else
    {
        return closest;
    }
}

static struct kan_avl_tree_node_t *get_lowest_in_subtree (struct kan_avl_tree_node_t *current)
{
    while (current->left)
    {
        current = current->left;
    }

    return current;
}

CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_ascending_iteration_begin (struct kan_avl_tree_t *tree)
{
    if (!tree->root)
    {
        return NULL;
    }

    return get_lowest_in_subtree (tree->root);
}

CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_ascending_iteration_next (struct kan_avl_tree_node_t *current)
{
    if (current->right)
    {
        return get_lowest_in_subtree (current->right);
    }

    struct kan_avl_tree_node_t *parent = current->parent;
    struct kan_avl_tree_node_t *child = current;

    while (parent)
    {
        if (parent->right != child)
        {
            break;
        }

        child = parent;
        parent = parent->parent;
    }

    return parent;
}

static struct kan_avl_tree_node_t *get_greatest_in_subtree (struct kan_avl_tree_node_t *current)
{
    while (current->right)
    {
        current = current->right;
    }

    return current;
}

CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_descending_iteration_begin (struct kan_avl_tree_t *tree)
{
    if (!tree->root)
    {
        return NULL;
    }

    return get_greatest_in_subtree (tree->root);
}

CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_descending_iteration_next (struct kan_avl_tree_node_t *current)
{
    if (current->left)
    {
        return get_greatest_in_subtree (current->left);
    }

    struct kan_avl_tree_node_t *parent = current->parent;
    struct kan_avl_tree_node_t *child = current;

    while (parent)
    {
        if (parent->left != child)
        {
            break;
        }

        child = parent;
        parent = parent->parent;
    }

    return parent;
}

static void retrace_on_delete (struct kan_avl_tree_t *tree,
                               struct kan_avl_tree_node_t *retrace_parent,
                               struct kan_avl_tree_node_t *retrace_child)
{
    while (retrace_parent)
    {
        if (retrace_parent->left == retrace_child)
        {
            switch (retrace_parent->balance_factor)
            {
            case -1:
                retrace_parent->balance_factor = 0;
                break;

            case 0:
                retrace_parent->balance_factor = 1;
                return;

            case 1:
            {
                switch (retrace_parent->right->balance_factor)
                {
                case -1:
                {
                    KAN_ASSERT (retrace_parent->right->left)
                    int8_t deep_balance_factor = retrace_parent->right->left->balance_factor;
                    right_rotate (tree, retrace_parent->right);
                    retrace_parent = left_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0u;

                    switch (deep_balance_factor)
                    {
                    case -1:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 1;
                        break;

                    case 0:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    case 1:
                        retrace_parent->left->balance_factor = -1;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    default:
                        KAN_ASSERT (KAN_FALSE)
                        break;
                    }

                    break;
                }

                case 0:
                    retrace_parent = left_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = -1;
                    retrace_parent->left->balance_factor = 1;
                    break;

                case 1:
                    retrace_parent = left_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;
                    retrace_parent->left->balance_factor = 0;
                    break;

                default:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                if (retrace_parent->balance_factor == -1)
                {
                    return;
                }

                break;
            }

            default:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }
        else
        {
            KAN_ASSERT (retrace_parent->right == retrace_child)
            switch (retrace_parent->balance_factor)
            {
            case -1:
            {
                switch (retrace_parent->left->balance_factor)
                {
                case -1:
                    retrace_parent = right_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0;
                    retrace_parent->right->balance_factor = 0;
                    break;

                case 0:
                    retrace_parent = right_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 1;
                    retrace_parent->right->balance_factor = -1;
                    break;

                case 1:
                {
                    KAN_ASSERT (retrace_parent->left->right)
                    int8_t deep_balance_factor = retrace_parent->left->right->balance_factor;
                    left_rotate (tree, retrace_parent->left);
                    retrace_parent = right_rotate (tree, retrace_parent);
                    retrace_parent->balance_factor = 0u;

                    switch (deep_balance_factor)
                    {
                    case -1:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 1;
                        break;

                    case 0:
                        retrace_parent->left->balance_factor = 0;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    case 1:
                        retrace_parent->left->balance_factor = -1;
                        retrace_parent->right->balance_factor = 0;
                        break;

                    default:
                        KAN_ASSERT (KAN_FALSE)
                        break;
                    }

                    break;
                }

                default:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                if (retrace_parent->balance_factor == 1)
                {
                    return;
                }

                break;
            }

            case 0:
                retrace_parent->balance_factor = -1;
                return;

            case 1:
                retrace_parent->balance_factor = 0;
                break;

            default:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }

        retrace_child = retrace_parent;
        retrace_parent = retrace_parent->parent;
    }
}

void kan_avl_tree_delete (struct kan_avl_tree_t *tree, struct kan_avl_tree_node_t *node)
{
    --tree->size;
    if (node->left && node->right)
    {
        struct kan_avl_tree_node_t *lowest = get_lowest_in_subtree (node->right);
        struct kan_avl_tree_node_t *lowest_parent = lowest->parent;
        struct kan_avl_tree_node_t *lowest_right = lowest->right;
        int8_t lowest_factor = lowest->balance_factor;

        lowest->parent = node->parent;
        lowest->balance_factor = node->balance_factor;

        if (node->parent)
        {
            if (node == node->parent->left)
            {
                node->parent->left = lowest;
            }
            else
            {
                KAN_ASSERT (node == node->parent->right)
                node->parent->right = lowest;
            }
        }
        else
        {
            tree->root = lowest;
        }

        if (lowest_parent == node)
        {
            lowest->right = node;
            lowest->left = node->left;

            if (lowest->left)
            {
                lowest->left->parent = lowest;
            }

            node->right = lowest_right;
            node->left = NULL;
            node->balance_factor = lowest_factor;

            if (lowest_right)
            {
                lowest_right->parent = node;
            }

            node->parent = lowest;
        }
        else
        {
            lowest->right = node->right;
            if (lowest->right)
            {
                lowest->right->parent = lowest;
            }

            lowest->left = node->left;
            if (lowest->left)
            {
                lowest->left->parent = lowest;
            }

            node->left = NULL;
            node->right = lowest_right;
            node->balance_factor = lowest_factor;

            node->parent = lowest_parent;
            KAN_ASSERT (lowest_parent && lowest_parent->left == lowest)
            lowest_parent->left = node;
        }
    }

    retrace_on_delete (tree, node->parent, node);
    KAN_ASSERT (!node->left || !node->right)
    struct kan_avl_tree_node_t *child = node->left ? node->left : node->right;

    if (child)
    {
        child->parent = node->parent;
    }

    if (node->parent)
    {
        if (node == node->parent->left)
        {
            node->parent->left = child;
        }
        else
        {
            KAN_ASSERT (node == node->parent->right)
            node->parent->right = child;
        }
    }
    else
    {
        tree->root = child;
    }
}
