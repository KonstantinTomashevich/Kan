#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains implementation for AVL tree data structure.
///
/// \par Definition
/// \parblock
/// AVL tree is a binary search tree with automatic balancing.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// This AVL tree implementation does not allocate and deallocate any memory and expects user to take care of all
/// required allocations and deallocations. This makes it possible for user to select the best memory strategies for
/// concrete algorithms and use cases.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Tree can be allocated anywhere as `kan_avl_tree_t` and then initialized using `kan_avl_tree_init`.
///
/// Insertion consists of two operations `kan_avl_tree_find_parent_for_insertion` and  `kan_avl_tree_insert` with
/// `kan_avl_tree_can_insert` helper. It is done so user can query for parent node and skip insertion if it is not
/// possible due to existence of the node with the same value. When insertion is skipped that way, user skips allocation
/// of the new node.
///
/// Example of tree insertion:
/// ```c
/// // Declare your node structure.
/// struct my_node_t
/// {
///     struct kan_avl_tree_node_t node;
///     struct my_data_t data;
/// };
///
/// // Create allocator for your node structure.
/// struct my_node_t *allocate_my_node ();
///
/// // Check if insertion is possible.
/// struct kan_avl_tree_node_t *parent = kan_avl_tree_find_parent_for_insertion (&my_tree, my_tree_value);
///
/// if (kan_avl_tree_can_insert (parent, my_tree_value))
/// {
///     // Insert new node.
///     struct my_node_t *node = allocate_my_node ();
///     // You only need to initialize tree value.
///     node->node.tree_value = my_tree_value;
///     kan_avl_tree_insert (&my_tree, parent, node);
/// }
/// else
/// {
///     // Insertion is not possible, but you can use parent pointer to merge or overwrite data.
/// }
/// ```
///
/// Deletion is straightforward: once you have pointer to
/// node that needs to be deleted, just call `kan_avl_tree_delete`.
///
/// There is 3 supported lookups:
/// - `kan_avl_tree_find_equal` that searches for the node with the exact tree value.
/// - `kan_avl_tree_find_upper_bound` that searches for the node with smallest value that is greater than given value.
/// - `kan_avl_tree_find_lower_bound` that searches for the node with greatest value that is smaller than given value.
///
/// Also, you can iterate over nodes using `kan_avl_tree_ascending_iteration_next` and
/// `kan_avl_tree_descending_iteration_next`. `kan_avl_tree_ascending_iteration_begin` and
/// `kan_avl_tree_descending_iteration_begin` provide you with initial iterators, but you can also start iteration
/// from any node, returned by the lookups above.
///
/// There is no shutdown operation for the tree, because tree does not allocate any memory by itself. Nevertheless,
/// user should iterate over allocated tree nodes and deallocate them using appropriate iterator.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// - Operations, that do not modify the tree, for example iteration and lookups, are thread safe.
/// - Insertion and deletion operations modify the tree and therefore are not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Base structure for AVL tree nodes.
struct kan_avl_tree_node_t
{
    uint64_t tree_value;
    struct kan_avl_tree_node_t *parent;
    struct kan_avl_tree_node_t *left;
    struct kan_avl_tree_node_t *right;
    int8_t balance_factor;
};

/// \brief Root structure of an AVL tree.
struct kan_avl_tree_t
{
    struct kan_avl_tree_node_t *root;
    uint64_t size;
};

/// \brief Initializes given AVL tree at given address.
CONTAINER_API void kan_avl_tree_init (struct kan_avl_tree_t *tree);

/// \brief Searches for appropriate parent for inserting node with given tree value.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_find_parent_for_insertion (struct kan_avl_tree_t *tree,
                                                                                  uint64_t tree_value);

/// \brief Checks whether it is possible to insert node with given tree value at given parent.
/// \details Use `kan_avl_tree_find_parent_for_insertion` to get parent node before calling this function.
static inline kan_bool_t kan_avl_tree_can_insert (struct kan_avl_tree_node_t *found_parent, uint64_t tree_value)
{
    return !found_parent || found_parent->tree_value != tree_value;
}

/// \brief Inserts new node with given tree value at given parent.
/// \details Use `kan_avl_tree_find_parent_for_insertion` to get parent node before calling this function.
/// \invariant `kan_avl_tree_can_insert` must be true.
CONTAINER_API void kan_avl_tree_insert (struct kan_avl_tree_t *tree,
                                        struct kan_avl_tree_node_t *found_parent,
                                        struct kan_avl_tree_node_t *node);

/// \brief Searches for the node with exact tree value.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_find_equal (struct kan_avl_tree_t *tree, uint64_t tree_value);

/// \brief Searches for the node with smallest value that is greater than given value.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_find_upper_bound (struct kan_avl_tree_t *tree,
                                                                         uint64_t tree_value);

/// \brief Searches for the node with greatest value that is smaller than given value.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_find_lower_bound (struct kan_avl_tree_t *tree,
                                                                         uint64_t tree_value);

/// \brief Returns smallest node in the AVL tree.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_ascending_iteration_begin (struct kan_avl_tree_t *tree);

/// \brief Moves to the next node in the ascending order.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_ascending_iteration_next (struct kan_avl_tree_node_t *current);

/// \brief Returns greatest node in the AVL tree.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_descending_iteration_begin (struct kan_avl_tree_t *tree);

/// \brief Moves to the next node in the descending order.
CONTAINER_API struct kan_avl_tree_node_t *kan_avl_tree_descending_iteration_next (struct kan_avl_tree_node_t *current);

/// \brief Deletes given node from AVL tree.
CONTAINER_API void kan_avl_tree_delete (struct kan_avl_tree_t *tree, struct kan_avl_tree_node_t *node);

KAN_C_HEADER_END
