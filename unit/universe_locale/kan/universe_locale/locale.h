#pragma once

#include <universe_locale_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/resource_locale/locale.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Provides API for interacting with locale management implementation.
///
/// \par Definition
/// \parblock
/// Locale management automatically loads all information about available locales and languages from the resources.
/// Loading is guaranteed to be done for the complete locale and language setup at once, even during hot reload, 
/// therefore there is not partial broken information stages. It also updates provided data by calculating 
/// locale-specific information if locale is selected.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all locale management mutators.
#define KAN_RENDER_LOCALE_MANAGEMENT_MUTATOR_GROUP "render_locale_management"

/// \brief Checkpoint, after which locale management mutators are executed.
#define KAN_RENDER_LOCALE_MANAGEMENT_BEGIN_CHECKPOINT "render_locale_management_begin"

/// \brief Checkpoint, that is hit after all locale management mutators have finished execution.
#define KAN_RENDER_LOCALE_MANAGEMENT_END_CHECKPOINT "render_locale_management_end"

/// \brief Singleton for selecting current locale and checking whether locale loading was done at least once.
struct kan_locale_singleton_t
{
    kan_interned_string_t selected_locale;
    
    /// \brief True if locale loading was fully completed at least once.
    bool initial_loading_complete;
};

UNIVERSE_LOCALE_API void kan_locale_singleton_init (struct kan_locale_singleton_t *instance);

/// \brief Event that is automatically sent if `kan_locale_singleton_t::selected_locale` is changed by anyone.
struct kan_locale_selection_updated_t
{
    kan_interned_string_t old_selection;
    kan_interned_string_t new_selection;
};

/// \brief Stores information about loaded locale setup.
struct kan_locale_t
{
    kan_interned_string_t name;
    struct kan_resource_locale_t resource;
};

UNIVERSE_LOCALE_API void kan_locale_init (struct kan_locale_t *instance);

UNIVERSE_LOCALE_API void kan_locale_shutdown (struct kan_locale_t *instance);

/// \brief Stores information about loaded language setup.
struct kan_language_t
{
    kan_interned_string_t name;
    
    /// \brief True if language should be loaded for fonts with current locale (if locale is selected at all).
    bool allowed_to_be_used_for_fonts;
    
    struct kan_resource_language_t resource;
};

UNIVERSE_LOCALE_API void kan_language_init (struct kan_language_t *instance);

/// \brief Event that is fired when locale information is changed either due to hot reload or selected locale change.
struct kan_locale_update_applied_t
{
    /// \brief Event has no specific data, so we need a stub field.
    kan_instance_size_t stub;
};

KAN_C_HEADER_END
