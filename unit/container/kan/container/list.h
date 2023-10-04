#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains trivial implementation of bidirectional linked list.
///
/// \par Definition
/// \parblock
/// Linked list is a simplistic container that stores data in nodes that point to each other in sequential manner.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// This linked list implementation does not controls any allocations and expects user to correctly allocate required
/// memory blocks. It makes it possible for user to control every aspect of allocation without making linked list
/// implementation excessively complex.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Linked list can be allocated anywhere as `kan_bd_list_t` and then initialized using `kan_bd_list_init`.
///
/// To add new node to the list, user must allocate compatible node structure using any allocator and then call
/// `kan_bd_list_add` function, for example:
///
/// ```c
/// // Firstly, define your node structure that starts with list node data.
/// struct my_node_t
/// {
///     struct kan_bd_list_node_t node;
///     struct my_data_t data;
/// };
///
/// // Then define allocator function for your nodes.
/// struct my_node_t *allocate_my_node ();
///
/// // Then you can create new node like that. No modifications to list node (field `node`) part are needed.
/// struct my_node_t *node = allocate_my_node ();
/// // And then it can be added to the list.
/// kan_bd_list_add (&list, &insert_before_this_node->node, &node->node);
/// ```
///
/// Removal is pretty straightforward and can be done using `kan_bd_list_remove`,
/// but do not forget to free node resources.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// This implementation of bidirectional linked list is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains bidirectional linked list node data.
struct kan_bd_list_node_t
{
    struct kan_bd_list_node_t *next;
    struct kan_bd_list_node_t *previous;
};

/// \brief Contains bidirectional linked list data.
struct kan_bd_list_t
{
    uint64_t size;
    struct kan_bd_list_node_t *first;
    struct kan_bd_list_node_t *last;
};

/// \brief Initializes given bidirectional linked list at given address.
CONTAINER_API void kan_bd_list_init (struct kan_bd_list_t *list);

/// \brief Inserts new node before given node of the linked list. If `NULL` is given, inserts new node at the end.
CONTAINER_API void kan_bd_list_add (struct kan_bd_list_t *list,
                                    struct kan_bd_list_node_t *before,
                                    struct kan_bd_list_node_t *node);

/// \brief Removes given node from the linked list.
CONTAINER_API void kan_bd_list_remove (struct kan_bd_list_t *list, struct kan_bd_list_node_t *node);

KAN_C_HEADER_END
