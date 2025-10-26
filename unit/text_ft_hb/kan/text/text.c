#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <ft2build.h>

#include <freetype/freetype.h>
#include <freetype/ftmm.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftsizes.h>
#include <freetype/ftsystem.h>

#include <hb.h>

#include <unicode/ubrk.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/text/text.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (text);

#define TO_26_6(VALUE) ((VALUE) * 64)
#define FROM_26_6(VALUE) ((float) (VALUE) / 64.0f)
#define MISSING_GLYPH 0u

KAN_USE_STATIC_CPU_SECTIONS
static struct kan_atomic_int_t statics_initialization_lock = {0};
static bool statics_initialized = false;

static kan_allocation_group_t main_allocation_group;
static kan_allocation_group_t text_allocation_group;
static kan_allocation_group_t freetype_allocation_group;
static kan_allocation_group_t harfbuzz_allocation_group;
static kan_allocation_group_t font_library_allocation_group;
static kan_allocation_group_t shaping_temporary_allocation_group;

static FT_Library freetype_library = NULL;

#if defined(KAN_TEXT_FT_HB_PROFILE_MEMORY)
void *freetype_alloc (FT_Memory memory, long size)
{
    const kan_memory_size_t initial_size = size;
    const kan_memory_size_t allocation_size =
        sizeof (kan_memory_size_t) + kan_apply_alignment (initial_size, alignof (kan_memory_size_t));

    kan_memory_size_t *allocation =
        kan_allocate_general (freetype_allocation_group, allocation_size, alignof (kan_memory_size_t));

    *allocation = initial_size;
    return allocation + 1u;
}

void freetype_free (FT_Memory memory, void *ptr)
{
    if (ptr)
    {
        kan_memory_size_t *old_allocation = ((kan_memory_size_t *) ptr) - 1u;
        const kan_memory_size_t allocation_size =
            sizeof (kan_memory_size_t) + kan_apply_alignment (*old_allocation, alignof (kan_memory_size_t));
        kan_free_general (freetype_allocation_group, old_allocation, allocation_size);
    }
}

void *freetype_realloc (FT_Memory memory, long current_size, long new_size, void *ptr)
{
    if (!ptr)
    {
        return freetype_alloc (memory, new_size);
    }
    else if (!new_size)
    {
        freetype_free (memory, ptr);
        return NULL;
    }
    else
    {
        void *new_memory = freetype_alloc (memory, new_size);
        memcpy (new_memory, ptr, KAN_MIN (current_size, new_size));
        freetype_free (memory, ptr);
        return new_memory;
    }
}

static struct FT_MemoryRec_ freetype_memory = {
    .user = NULL,
    .alloc = freetype_alloc,
    .free = freetype_free,
    .realloc = freetype_realloc,
};
#endif

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        KAN_ATOMIC_INT_SCOPED_LOCK (&statics_initialization_lock)
        if (statics_initialized)
        {
            return;
        }

        kan_cpu_static_sections_ensure_initialized ();
        main_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "text");
        text_allocation_group = kan_allocation_group_get_child (main_allocation_group, "text");
        freetype_allocation_group = kan_allocation_group_get_child (main_allocation_group, "freetype");
        harfbuzz_allocation_group = kan_allocation_group_get_child (main_allocation_group, "harfbuzz");
        font_library_allocation_group = kan_allocation_group_get_child (main_allocation_group, "font_library");
        shaping_temporary_allocation_group =
            kan_allocation_group_get_child (main_allocation_group, "shaping_temporary");
        statics_initialized = true;
    }
}

#if defined(KAN_TEXT_FT_HB_PROFILE_MEMORY)
void *hb_malloc_impl (size_t size)
{
    const kan_memory_size_t initial_size = size;
    const kan_memory_size_t allocation_size =
        sizeof (kan_memory_size_t) + kan_apply_alignment (initial_size, alignof (kan_memory_size_t));

    kan_memory_size_t *allocation =
        kan_allocate_general (harfbuzz_allocation_group, allocation_size, alignof (kan_memory_size_t));

    *allocation = initial_size;
    return allocation + 1u;
}

void *hb_calloc_impl (size_t nmemb, size_t size)
{
    void *memory = hb_malloc_impl (nmemb * size);
    memset (memory, 0, nmemb * size);
    return memory;
}

void *hb_realloc_impl (void *ptr, size_t size)
{
    if (!ptr)
    {
        return hb_malloc_impl (size);
    }
    else if (!size)
    {
        hb_free (ptr);
        return NULL;
    }
    else
    {
        kan_memory_size_t *old_allocation = ((kan_memory_size_t *) ptr) - 1u;
        void *new_memory = hb_malloc_impl (size);
        memcpy (new_memory, ptr, KAN_MIN (*old_allocation, size));
        hb_free (ptr);
        return new_memory;
    }
}

void hb_free_impl (void *ptr)
{
    if (ptr)
    {
        kan_memory_size_t *old_allocation = ((kan_memory_size_t *) ptr) - 1u;
        const kan_memory_size_t allocation_size =
            sizeof (kan_memory_size_t) + kan_apply_alignment (*old_allocation, alignof (kan_memory_size_t));
        kan_free_general (harfbuzz_allocation_group, old_allocation, allocation_size);
    }
}
#else
void *hb_malloc_impl (size_t size) { return malloc (size); }
void *hb_calloc_impl (size_t nmemb, size_t size) { return calloc (nmemb, size); }
void *hb_realloc_impl (void *ptr, size_t size) { return realloc (ptr, size); }
void hb_free_impl (void *ptr) { free (ptr); }
#endif

enum text_node_type_t
{
    TEXT_NODE_TYPE_UTF8 = 0u,
    TEXT_NODE_TYPE_ICON,
    TEXT_NODE_TYPE_STYLE,
};

struct text_utf8_t
{
    hb_script_t script;
    kan_instance_size_t length;
    char data[];
};

struct text_icon_t
{
    kan_interned_string_t style;
    uint32_t icon_index;
    uint32_t base_codepoint;
    float x_scale;
    float y_scale;
};

struct text_style_t
{
    kan_interned_string_t style;
    uint32_t mark_index;
};

struct text_node_t
{
    struct text_node_t *next;
    enum text_node_type_t type;

    union
    {
        struct text_utf8_t utf8;
        struct text_icon_t icon;
        struct text_style_t style;
    };
};

struct text_create_context_t
{
    struct text_node_t *first_node;
    struct text_node_t *last_node;
    hb_script_t current_script;
    kan_instance_size_t first_uncommited_utf8_index;
    kan_instance_size_t first_uncommited_utf8_offset;
};

static inline void text_commit_trailing_utf8 (struct text_create_context_t *context,
                                              kan_instance_size_t items_count,
                                              struct kan_text_item_t *items,
                                              kan_instance_size_t current_index,
                                              kan_instance_size_t current_start,
                                              kan_instance_size_t current_end)
{
    kan_instance_size_t data_length = 0u;
    if (context->first_uncommited_utf8_index != KAN_INT_MAX (kan_instance_size_t))
    {
        for (kan_loop_size_t uncommited_index = context->first_uncommited_utf8_index; uncommited_index < current_index;
             ++uncommited_index)
        {
            struct kan_text_item_t *uncommited_item = &items[uncommited_index];
            KAN_ASSERT (uncommited_item->type == KAN_TEXT_ITEM_UTF8)

            const kan_instance_size_t from_offset =
                uncommited_index == context->first_uncommited_utf8_index ? context->first_uncommited_utf8_offset : 0u;

            const kan_instance_size_t length = strlen (uncommited_item->utf8);
            data_length += length - from_offset;
        }
    }

    if (current_index != items_count)
    {
        struct kan_text_item_t *current_item = &items[current_index];
        if (current_item->type == KAN_TEXT_ITEM_UTF8)
        {
            data_length += current_end - current_start;
        }
    }

    KAN_ASSERT (data_length > 0u)
    struct text_node_t *node = kan_allocate_general (
        text_allocation_group,
        kan_apply_alignment (sizeof (struct text_node_t) + data_length + 1u, alignof (struct text_node_t)),
        alignof (struct text_node_t));

    node->next = NULL;
    node->type = TEXT_NODE_TYPE_UTF8;
    node->utf8.script = context->current_script;
    node->utf8.length = data_length;
    kan_instance_size_t write_offset = 0u;

    if (context->first_uncommited_utf8_index != KAN_INT_MAX (kan_instance_size_t))
    {
        for (kan_loop_size_t uncommited_index = context->first_uncommited_utf8_index; uncommited_index < current_index;
             ++uncommited_index)
        {
            struct kan_text_item_t *uncommited_item = &items[uncommited_index];
            KAN_ASSERT (uncommited_item->type == KAN_TEXT_ITEM_UTF8)

            const kan_instance_size_t from_offset =
                uncommited_index == context->first_uncommited_utf8_index ? context->first_uncommited_utf8_offset : 0u;

            const kan_instance_size_t length = strlen (uncommited_item->utf8);
            memcpy (node->utf8.data + write_offset, uncommited_item->utf8, length - from_offset);
            write_offset += length - from_offset;
        }

        context->first_uncommited_utf8_index = KAN_INT_MAX (kan_instance_size_t);
    }

    if (current_index != items_count && current_start != current_end)
    {
        struct kan_text_item_t *current_item = &items[current_index];
        if (current_item->type == KAN_TEXT_ITEM_UTF8)
        {
            memcpy (node->utf8.data + write_offset, current_item->utf8 + current_start, current_end - current_start);
            write_offset += current_end - current_start;
        }
    }

    node->utf8.data[write_offset] = '\0';
    if (context->last_node)
    {
        context->last_node->next = node;
    }
    else
    {
        context->first_node = node;
    }

    context->last_node = node;
}

kan_text_t kan_text_create (kan_instance_size_t items_count, struct kan_text_item_t *items)
{
    ensure_statics_initialized ();
    KAN_CPU_SCOPED_STATIC_SECTION (kan_text_create)
    struct text_create_context_t context = {
        .first_node = NULL,
        .last_node = NULL,
        .current_script = HB_SCRIPT_UNKNOWN,
        .first_uncommited_utf8_index = KAN_INT_MAX (kan_instance_size_t),
        .first_uncommited_utf8_offset = 0u,
    };

    kan_interned_string_t style = NULL;
    uint32_t mark_index = 0u;

    for (kan_loop_size_t index = 0u; index < items_count; ++index)
    {
        struct kan_text_item_t *item = &items[index];
        switch (item->type)
        {
        case KAN_TEXT_ITEM_EMPTY:
            // Do nothing.
            break;

        case KAN_TEXT_ITEM_UTF8:
        {
            const uint8_t *utf8 = (uint8_t *) item->utf8;
            int32_t offset = 0u;
            UChar32 codepoint = 0;
            kan_instance_size_t uncommited_from = 0u;

            while (true)
            {
                const int32_t pre_step_offset = offset;
                U8_NEXT (utf8, offset, -1, codepoint);

                if (codepoint <= 0)
                {
                    break;
                }

                const hb_script_t script =
                    hb_unicode_script (hb_unicode_funcs_get_default (), (hb_codepoint_t) codepoint);

                switch (script)
                {
                case HB_SCRIPT_COMMON:
                case HB_SCRIPT_INHERITED:
                case HB_SCRIPT_UNKNOWN:
                    // Common scripts are treated as part of primary scripts.
                    break;

                default:
                    if (script != context.current_script)
                    {
                        switch (context.current_script)
                        {
                        case HB_SCRIPT_COMMON:
                        case HB_SCRIPT_INHERITED:
                        case HB_SCRIPT_UNKNOWN:
                            // Common scripts are treated as part of primary scripts.
                            break;

                        default:
                        {
                            text_commit_trailing_utf8 (&context, items_count, items, index, uncommited_from,
                                                       (kan_instance_size_t) pre_step_offset);
                            uncommited_from = (kan_instance_size_t) pre_step_offset;
                            break;
                        }
                        }
                    }

                    context.current_script = script;
                    break;
                }
            }

            KAN_ASSERT (context.first_uncommited_utf8_index == KAN_INT_MAX (kan_instance_size_t) ||
                        uncommited_from == 0u)

            if (context.first_uncommited_utf8_index == KAN_INT_MAX (kan_instance_size_t))
            {
                context.first_uncommited_utf8_index = index;
                context.first_uncommited_utf8_offset = uncommited_from;
            }

            break;
        }

        case KAN_TEXT_ITEM_ICON:
        {
            if (context.first_uncommited_utf8_index != KAN_INT_MAX (kan_instance_size_t))
            {
                text_commit_trailing_utf8 (&context, items_count, items, index, 0u, 0u);
                context.current_script = HB_SCRIPT_UNKNOWN;
            }

            struct text_node_t *node =
                kan_allocate_general (text_allocation_group, sizeof (struct text_node_t), alignof (struct text_node_t));

            node->next = NULL;
            node->type = TEXT_NODE_TYPE_ICON;
            node->icon.icon_index = item->icon.icon_index;
            node->icon.base_codepoint = item->icon.base_codepoint;
            node->icon.x_scale = item->icon.x_scale;
            node->icon.y_scale = item->icon.y_scale;

            if (context.last_node)
            {
                context.last_node->next = node;
            }
            else
            {
                context.first_node = node;
            }

            context.last_node = node;
            break;
        }

        case KAN_TEXT_ITEM_STYLE:
        {
            if (item->style.style == style && item->style.mark_index == mark_index)
            {
                break;
            }

            style = item->style.style;
            mark_index = item->style.mark_index;

            if (context.first_uncommited_utf8_index != KAN_INT_MAX (kan_instance_size_t))
            {
                text_commit_trailing_utf8 (&context, items_count, items, index, 0u, 0u);
                context.current_script = HB_SCRIPT_UNKNOWN;
            }

            struct text_node_t *node =
                kan_allocate_general (text_allocation_group, sizeof (struct text_node_t), alignof (struct text_node_t));

            node->next = NULL;
            node->type = TEXT_NODE_TYPE_STYLE;
            node->style.style = item->style.style;
            node->style.mark_index = item->style.mark_index;

            if (context.last_node)
            {
                context.last_node->next = node;
            }
            else
            {
                context.first_node = node;
            }

            context.last_node = node;
            break;
        }
        }
    }

    if (context.first_uncommited_utf8_index != KAN_INT_MAX (kan_instance_size_t))
    {
        text_commit_trailing_utf8 (&context, items_count, items, items_count, 0u, 0u);
    }

    return KAN_HANDLE_SET (kan_text_t, context.first_node);
}

void kan_text_destroy (kan_text_t instance)
{
    struct text_node_t *node = KAN_HANDLE_GET (instance);
    while (node)
    {
        struct text_node_t *next = node->next;
        switch (node->type)
        {
        case TEXT_NODE_TYPE_UTF8:
            kan_free_general (text_allocation_group, node,
                              kan_apply_alignment (sizeof (struct text_node_t) + node->utf8.length + 1u,
                                                   alignof (struct text_node_t)));
            break;

        case TEXT_NODE_TYPE_ICON:
        case TEXT_NODE_TYPE_STYLE:
            kan_free_general (text_allocation_group, node, sizeof (struct text_node_t));
            break;
        }

        node = next;
    }
}

void kan_text_shaped_data_init (struct kan_text_shaped_data_t *instance)
{
    instance->min.x = 0;
    instance->min.y = 0;
    instance->max.x = 0;
    instance->max.y = 0;

    kan_dynamic_array_init (&instance->glyphs, 0u, sizeof (struct kan_text_shaped_glyph_instance_data_t),
                            alignof (struct kan_text_shaped_glyph_instance_data_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->icons, 0u, sizeof (struct kan_text_shaped_icon_instance_data_t),
                            alignof (struct kan_text_shaped_icon_instance_data_t), kan_allocation_group_stack_get ());
}

void kan_text_shaped_data_shutdown (struct kan_text_shaped_data_t *instance)
{
    kan_dynamic_array_shutdown (&instance->glyphs);
    kan_dynamic_array_shutdown (&instance->icons);
}

struct font_rendered_glyph_node_t
{
    struct font_rendered_glyph_node_t *next;
    enum kan_font_glyph_render_format_t format;
    kan_instance_size_t layer;
    struct kan_int32_vector_2_t bitmap_bearing;
    struct kan_int32_vector_2_t bitmap_size;
    struct kan_float_vector_2_t uv_min;
    struct kan_float_vector_2_t uv_max;
};

struct font_glyph_node_t
{
    struct kan_hash_storage_node_t node;
    kan_instance_size_t glyph_index;
    struct font_rendered_glyph_node_t *rendered_first;
};

struct font_library_category_t
{
    kan_interned_string_t style;
    hb_script_t script;

    // We create faces separately and do not share memory between them as freetype is not multithreaded, but we'd like
    // to use multithreading for shaping whenever possible.

    FT_Face freetype_face;

    hb_blob_t *harfbuzz_face_blob;
    hb_face_t *harfbuzz_face;

    kan_instance_size_t variable_axis_count;
    float *variable_axis;

    struct kan_atomic_int_t glyphs_read_write_lock;
    struct kan_hash_storage_t glyphs;
};

struct font_library_sdf_atlas_t
{
    kan_render_image_t image;
    kan_instance_size_t current_layer;

    // We use very simplistic and space-inefficient packing: just put everything in a row and use last row max height
    // as step for calculating y offset for the whole next row. But it is very fast to calculate next coordinate and
    // space waste should not be as noticeable, because SDF glyph sizes should be relatively equal to each other.

    kan_instance_size_t current_row_x;
    kan_instance_size_t current_row_y;
    kan_instance_size_t current_row_max_height;
};

struct font_library_t
{
    kan_render_context_t render_context;
    struct kan_atomic_int_t freetype_lock;

    /// \details Freetype states that freetype objects can only be accessed from multiple threads if they belong to
    ///          different libraries. We'd like font library creation to be multithreading-safe, so we create separate
    ///          freetype libraries. It should be okay as we do not really expect several font libraries to be around
    ///          all the time.
    FT_Library freetype_library;

    struct font_library_sdf_atlas_t sdf_atlas;

    struct kan_atomic_int_t allocator_lock;
    struct kan_stack_group_allocator_t allocator;

    kan_instance_size_t categories_count;
    struct font_library_category_t categories[];
};

static const struct kan_render_clear_color_t sdf_atlas_clear_color = {
    .r = 0.0f,
    .g = 0.0f,
    .b = 0.0f,
    .a = 0.0f,
};

kan_font_library_t kan_font_library_create (kan_render_context_t render_context,
                                            kan_instance_size_t categories_count,
                                            struct kan_font_library_category_t *categories)
{
    ensure_statics_initialized ();
    KAN_CPU_SCOPED_STATIC_SECTION (kan_font_library_create)

    const kan_memory_size_t library_size =
        sizeof (struct font_library_t) + sizeof (struct font_library_category_t) * categories_count;
    struct font_library_t *library =
        kan_allocate_general (font_library_allocation_group, library_size, alignof (struct font_library_t));

    library->render_context = render_context;
    library->freetype_lock = kan_atomic_int_init (0);
    library->freetype_library = NULL;

    FT_Error freetype_error;
#if defined(KAN_TEXT_FT_HB_PROFILE_MEMORY)
    freetype_error = FT_New_Library (&freetype_memory, &freetype_library);
    FT_Add_Default_Modules (freetype_library);
    FT_Set_Default_Properties (freetype_library);
#else
    freetype_error = FT_Init_FreeType (&freetype_library);
#endif

    if (freetype_error != FT_Err_Ok)
    {
        KAN_LOG (text, KAN_LOG_ERROR, "Failed to create freetype library during font library creation.")
        kan_free_general (font_library_allocation_group, library, library_size);
        return KAN_HANDLE_SET_INVALID (kan_font_library_t);
    }

    struct kan_render_image_description_t sdf_atlas_description = {
        .format = KAN_RENDER_IMAGE_FORMAT_R8_UNORM,
        .width = KAN_TEXT_FT_HB_SDF_ATLAS_WIDTH,
        .height = KAN_TEXT_FT_HB_SDF_ATLAS_HEIGHT,
        .depth = 1u,
        .layers = KAN_TEXT_FT_HB_SDF_ATLAS_LAYERS,
        .mips = 1u,
        .render_target = false,
        .supports_sampling = true,
        .always_treat_as_layered = true,
        .tracking_name = kan_string_intern ("font_library_atlas"),
    };

    library->sdf_atlas.image = kan_render_image_create (render_context, &sdf_atlas_description);
    if (!KAN_HANDLE_IS_VALID (library->sdf_atlas.image))
    {
        KAN_LOG (text, KAN_LOG_ERROR, "Failed to allocate sdf atlas during font library creation.")
        FT_Done_Library (library->freetype_library);
        kan_free_general (font_library_allocation_group, library, library_size);
        return KAN_HANDLE_SET_INVALID (kan_font_library_t);
    }

    for (kan_loop_size_t layer = 0u; layer < KAN_TEXT_FT_HB_SDF_ATLAS_LAYERS; ++layer)
    {
        kan_render_image_clear_color (library->sdf_atlas.image, (kan_instance_size_t) layer, 0u,
                                      &sdf_atlas_clear_color);
    }

    library->sdf_atlas.current_layer = 0u;
    library->sdf_atlas.current_row_x = 0u;
    library->sdf_atlas.current_row_y = 0u;
    library->sdf_atlas.current_row_max_height = 0u;

    library->allocator_lock = kan_atomic_int_init (0);
    kan_stack_group_allocator_init (&library->allocator, font_library_allocation_group,
                                    KAN_TEXT_FT_HB_FONT_LIBRARY_STACK);
    library->categories_count = categories_count;

    struct kan_font_library_category_t *source = categories;
    struct font_library_category_t *target = library->categories;

    for (kan_loop_size_t index = 0u; index < library->categories_count; ++index, ++source, ++target)
    {
        target->script = hb_script_from_string (source->script, -1);
        target->style = source->style;

        target->freetype_face = NULL;
        target->harfbuzz_face_blob = NULL;
        target->harfbuzz_face = NULL;
        target->glyphs_read_write_lock = kan_atomic_int_init (0);
        kan_hash_storage_init (&target->glyphs, font_library_allocation_group, KAN_TEXT_FT_HB_FONT_LIBRARY_BUCKETS);

        freetype_error =
            FT_New_Memory_Face (freetype_library, source->data, source->data_size, 0u, &target->freetype_face);

        if (freetype_error != FT_Err_Ok)
        {
            KAN_LOG (text, KAN_LOG_ERROR, "Failed to create freetype face for font library category at index %u: %s",
                     (unsigned int) index, FT_Error_String (freetype_error))
            continue;
        }

        target->harfbuzz_face_blob =
            hb_blob_create (source->data, source->data_size, HB_MEMORY_MODE_READONLY, NULL, NULL);
        target->harfbuzz_face = hb_face_create_or_fail (target->harfbuzz_face_blob, 0);

        if (!target->harfbuzz_face)
        {
            KAN_LOG (text, KAN_LOG_ERROR, "Failed to create harfbuzz face for font library category at index %u: %s",
                     (unsigned int) index, FT_Error_String (freetype_error))
            continue;
        }

        target->variable_axis_count = source->variable_axis_count;
        if (source->variable_axis_count)
        {
            target->variable_axis = kan_stack_group_allocator_allocate (
                &library->allocator, sizeof (float) * source->variable_axis_count, alignof (float));
            memcpy (target->variable_axis, source->variable_axis, sizeof (float) * source->variable_axis_count);

            FT_Fixed *freetype_axis = kan_stack_group_allocator_allocate (
                &library->allocator, sizeof (FT_Fixed) * source->variable_axis_count, alignof (FT_Fixed));

            for (kan_loop_size_t axis_index = 0u; axis_index < source->variable_axis_count; ++axis_index)
            {
                freetype_axis[axis_index] = (FT_Fixed) roundf (source->variable_axis[axis_index] * 65536.0f);
            }

            FT_Set_Var_Design_Coordinates (target->freetype_face, (FT_UInt) target->variable_axis_count, freetype_axis);
        }
        else
        {
            target->variable_axis = NULL;
        }
    }

    return KAN_HANDLE_SET (kan_font_library_t, library);
}

kan_render_image_t kan_font_library_get_sdf_atlas (kan_font_library_t instance)
{
    struct font_library_t *library = KAN_HANDLE_GET (instance);
    return library->sdf_atlas.image;
}

static inline struct font_glyph_node_t *font_library_category_find_glyph_unsafe (
    struct font_library_category_t *category, kan_instance_size_t glyph_index)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&category->glyphs, (kan_hash_t) glyph_index);
    struct font_glyph_node_t *node = (struct font_glyph_node_t *) bucket->first;
    const struct font_glyph_node_t *node_end = (struct font_glyph_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->glyph_index == glyph_index)
        {
            return node;
        }

        node = (struct font_glyph_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline struct font_rendered_glyph_node_t *font_glyph_node_find_rendered (
    struct font_glyph_node_t *node, enum kan_font_glyph_render_format_t format)
{
    struct font_rendered_glyph_node_t *rendered = node->rendered_first;
    while (rendered)
    {
        if (rendered->format == format)
        {
            return rendered;
        }
    }

    return NULL;
}

struct icu_break_t
{
    uint32_t index;
    bool hard;
};

struct shape_sequence_t
{
    kan_instance_size_t first_glyph_index;
    kan_instance_size_t first_icon_index;
    int32_t length_26_6;
    int32_t biggest_line_space_26_6;
};

struct shape_render_delayed_reminder_t
{
    kan_instance_size_t font_glyph_index;
    kan_instance_size_t shaped_glyph_index;
};

struct shape_context_t
{
    struct font_library_t *library;
    struct kan_text_shaping_request_t *request;
    struct kan_text_shaped_data_t *output;

    hb_script_t script;
    kan_interned_string_t style;
    uint32_t mark_index;
    int32_t primary_axis_limit_26_6;

    struct font_library_category_t *current_category;
    hb_font_t *harfbuzz_font;
    hb_buffer_t *harfbuzz_buffer;

    struct kan_dynamic_array_t icu_breaks;
    struct kan_dynamic_array_t sequences;
    struct kan_dynamic_array_t render_delayed;
};

static bool shape_choose_category (struct shape_context_t *context)
{
    if (context->current_category)
    {
        KAN_ASSERT (context->harfbuzz_font)
        return true;
    }

    for (kan_loop_size_t index = 0u; index < context->library->categories_count; ++index)
    {
        struct font_library_category_t *category = &context->library->categories[index];
        if (!category->harfbuzz_face)
        {
            // Category is unusable.
            continue;
        }

        // If we're intentionally using common script, it might for things like icons,
        // therefore first available category should be usable for that case.
        if (context->script != HB_SCRIPT_COMMON &&
            (category->script != context->script || category->style != context->style))
        {
            continue;
        }

        context->current_category = category;
        context->harfbuzz_font = hb_font_create (category->harfbuzz_face);

        if (category->variable_axis)
        {
            hb_font_set_var_coords_design (context->harfbuzz_font, category->variable_axis,
                                           category->variable_axis_count);
        }

        hb_font_set_scale (context->harfbuzz_font, TO_26_6 (context->request->font_size),
                           TO_26_6 (context->request->font_size));
        hb_font_make_immutable (context->harfbuzz_font);
        return true;
    }

    char tag_buffer[5u];
    hb_tag_to_string (context->script, tag_buffer);
    tag_buffer[4u] = '\0';

    KAN_LOG (text, KAN_LOG_ERROR, "Failed to find font category for script \"%s\" and style \"%s\".", tag_buffer,
             context->style)
    return false;
}

static inline void shape_reset_category (struct shape_context_t *context)
{
    context->current_category = NULL;
    if (context->harfbuzz_font)
    {
        hb_font_destroy (context->harfbuzz_font);
        context->harfbuzz_font = NULL;
    }
}

/// \details Breaking is only allowed if script has the same direction as lines starts.
static inline bool shape_is_break_allowed (enum kan_text_reading_direction_t reading_direction,
                                           hb_direction_t script_direction)
{
    switch (reading_direction)
    {
    case KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT:
        return script_direction == HB_DIRECTION_LTR;

    case KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT:
        return script_direction == HB_DIRECTION_RTL;
    }

    return false;
}

static inline void shape_apply_rendered_data_to_glyph (struct shape_context_t *context,
                                                       struct kan_text_shaped_glyph_instance_data_t *glyph,
                                                       struct font_rendered_glyph_node_t *rendered)
{
    int32_t bearing_x = rendered->bitmap_bearing.x;
    int32_t bearing_y = rendered->bitmap_bearing.y;
    uint32_t width = rendered->bitmap_size.x;
    uint32_t height = rendered->bitmap_size.y;

    switch (context->request->render_format)
    {
    case KAN_FONT_GLYPH_RENDER_FORMAT_SDF:
        if (context->request->font_size != KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE)
        {
            const float scale = (float) context->request->font_size / (float) KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE;
            bearing_x = (int32_t) roundf (scale * (float) bearing_x);
            bearing_y = (int32_t) roundf (scale * (float) bearing_y);
            width = (int32_t) roundf (scale * (float) width);
            height = (int32_t) roundf (scale * (float) height);
        }

        break;
    }

    struct kan_int32_vector_2_t *min_26_6 = (struct kan_int32_vector_2_t *) &glyph->min;
    struct kan_int32_vector_2_t *max_26_6 = (struct kan_int32_vector_2_t *) &glyph->max;

    min_26_6->x += bearing_x;
    min_26_6->y += -bearing_y;
    max_26_6->x += bearing_x + width;
    max_26_6->y += height - bearing_y;

    glyph->uv_min = rendered->uv_min;
    glyph->uv_max = rendered->uv_max;
    glyph->layer = (uint32_t) rendered->layer;
}

static inline bool shape_extract_render_data_for_glyph_concurrently (
    struct shape_context_t *context,
    kan_instance_size_t glyph_index,
    struct kan_text_shaped_glyph_instance_data_t *glyph)
{
    KAN_ATOMIC_INT_SCOPED_LOCK_READ (&context->current_category->glyphs_read_write_lock)
    struct font_glyph_node_t *glyph_node =
        font_library_category_find_glyph_unsafe (context->current_category, glyph_index);

    if (glyph_node)
    {
        struct font_rendered_glyph_node_t *rendered =
            font_glyph_node_find_rendered (glyph_node, context->request->render_format);

        if (rendered)
        {
            shape_apply_rendered_data_to_glyph (context, glyph, rendered);
            return true;
        }
    }

    glyph->uv_min.x = 0.0f;
    glyph->uv_min.y = 0.0f;
    glyph->uv_max.x = 0.0f;
    glyph->uv_max.y = 0.0f;
    glyph->layer = 0u;
    return false;
}

static inline void shape_append_to_sequence (struct shape_context_t *context,
                                             struct shape_sequence_t *last_sequence,
                                             bool forward_processing,
                                             const hb_glyph_info_t *glyph_info,
                                             const hb_glyph_position_t *glyph_position)
{
    // After shaping data in glyph info is not an actual unicode codepoint, but glyph index in font.
    const kan_instance_size_t glyph_index = (kan_instance_size_t) glyph_info->codepoint;

    if (glyph_index == MISSING_GLYPH)
    {
        return;
    }

    struct kan_text_shaped_glyph_instance_data_t *shaped = kan_dynamic_array_add_last (&context->output->glyphs);
    if (!shaped)
    {
        kan_dynamic_array_set_capacity (&context->output->glyphs, context->output->glyphs.size * 2u);
        shaped = kan_dynamic_array_add_last (&context->output->glyphs);
    }

    int32_t origin_x = 0u;
    int32_t origin_y = 0u;

    switch (context->request->orientation)
    {
    case KAN_TEXT_ORIENTATION_HORIZONTAL:
        if (forward_processing)
        {
            origin_x = last_sequence->length_26_6 + glyph_position->x_offset;
            origin_y = glyph_position->y_offset;
        }
        else
        {
            origin_x = context->primary_axis_limit_26_6 - last_sequence->length_26_6 - glyph_position->x_advance +
                       glyph_position->x_offset;
            origin_y = glyph_position->y_offset;
        }

        last_sequence->length_26_6 += glyph_position->x_advance;
        break;

    case KAN_TEXT_ORIENTATION_VERTICAL:
        KAN_ASSERT (forward_processing) // No bottom-to-top ordering for now.
        origin_x = glyph_position->x_offset;
        origin_y = last_sequence->length_26_6 + glyph_position->y_offset;
        last_sequence->length_26_6 += glyph_position->y_advance;
        break;
    }

    // Just set min-max to the origin, bearing and size could only be properly applied once render data is ready,
    // because freetype data might be different due to different render mode (like SDF).
    struct kan_int32_vector_2_t *min_26_6 = (struct kan_int32_vector_2_t *) &shaped->min;
    struct kan_int32_vector_2_t *max_26_6 = (struct kan_int32_vector_2_t *) &shaped->max;
    min_26_6->x = origin_x;
    min_26_6->y = origin_y;
    max_26_6->x = origin_x;
    max_26_6->y = origin_y;

    if (!shape_extract_render_data_for_glyph_concurrently (context, glyph_index, shaped))
    {
        struct shape_render_delayed_reminder_t *render_delayed = kan_dynamic_array_add_last (&context->render_delayed);
        if (!render_delayed)
        {
            kan_dynamic_array_set_capacity (
                &context->render_delayed,
                KAN_MAX (context->render_delayed.size * 2u, KAN_TEXT_FT_HB_FONT_SHAPE_DELAYED_RENDER_BASE));
            render_delayed = kan_dynamic_array_add_last (&context->render_delayed);
        }

        render_delayed->font_glyph_index = glyph_index;
        render_delayed->shaped_glyph_index = context->output->glyphs.size - 1u;
    }
}

static void shape_render_sdf (struct shape_context_t *context,
                              struct font_rendered_glyph_node_t *rendered,
                              kan_instance_size_t font_glyph_index)
{
    FT_GlyphSlot slot = context->current_category->freetype_face->glyph;
    FT_Error freetype_error = FT_Render_Glyph (slot, FT_RENDER_MODE_SDF);

    if (freetype_error)
    {
        KAN_LOG (text, KAN_LOG_ERROR, "Failed to render glyph at index %lu with freetype.\n",
                 (unsigned long) font_glyph_index)
        return;
    }

    if (slot->bitmap.rows == 0u)
    {
        KAN_LOG (text, KAN_LOG_DEBUG, "Glyph at index %lu is represented by empty bitmap.\n",
                 (unsigned long) font_glyph_index)
        return;
    }

    KAN_ASSERT ((int) slot->bitmap.width == slot->bitmap.pitch)
    struct font_library_sdf_atlas_t *atlas = &context->library->sdf_atlas;

    kan_instance_size_t atlas_width;
    kan_instance_size_t atlas_height;
    kan_instance_size_t atlas_depth;
    kan_instance_size_t atlas_layers;
    kan_render_image_get_sizes (atlas->image, &atlas_width, &atlas_height, &atlas_depth, &atlas_layers);

    const kan_instance_size_t glyph_width = (kan_instance_size_t) slot->bitmap.width;
    const kan_instance_size_t glyph_height = (kan_instance_size_t) slot->bitmap.rows;
    KAN_ASSERT (glyph_width < atlas_width)
    KAN_ASSERT (glyph_height < atlas_height)

    if (atlas->current_row_x + glyph_width >= atlas_width)
    {
        // Start new row.
        atlas->current_row_x = 0u;
        atlas->current_row_y += atlas->current_row_max_height + KAN_TEXT_FT_HB_SDF_ATLAS_GLYPH_BORDER;
        atlas->current_row_max_height = 0u;
    }

    if (atlas->current_row_y + glyph_height >= atlas_height)
    {
        // Layer overflow, start new layer.
        atlas->current_row_x = 0u;
        atlas->current_row_y = 0u;
        atlas->current_row_max_height = 0u;
        ++atlas->current_layer;
    }

    if (atlas->current_layer >= atlas_layers)
    {
        // Atlas overflow. Reallocate it and copy data from old atlas properly.
        struct kan_render_image_description_t sdf_atlas_description = {
            .format = KAN_RENDER_IMAGE_FORMAT_R8_UNORM,
            .width = atlas_width,
            .height = atlas_height,
            .depth = atlas_depth,
            .layers = atlas_layers + KAN_TEXT_FT_HB_SDF_ATLAS_LAYER_STEP,
            .mips = 1u,
            .render_target = false,
            .supports_sampling = true,
            .always_treat_as_layered = true,
            .tracking_name = kan_string_intern ("font_library_atlas"),
        };

        kan_render_image_t new_atlas =
            kan_render_image_create (context->library->render_context, &sdf_atlas_description);

        if (!KAN_HANDLE_IS_VALID (new_atlas))
        {
            kan_error_critical ("Failed to allocate new atlas for font data, cannot recover properly from that.",
                                __FILE__, __LINE__);
        }

        for (kan_instance_size_t old_layer_index = 0u; old_layer_index < atlas_layers; ++old_layer_index)
        {
            kan_render_image_copy_data (atlas->image, old_layer_index, 0u, new_atlas, old_layer_index, 0u);
        }

        for (kan_loop_size_t layer = atlas_layers; layer < sdf_atlas_description.layers; ++layer)
        {
            kan_render_image_clear_color (atlas->image, (kan_instance_size_t) layer, 0u, &sdf_atlas_clear_color);
        }

        kan_render_image_destroy (atlas->image);
        atlas->image = new_atlas;
    }

    const struct kan_render_integer_region_3d_t region = {
        .x = (kan_instance_offset_t) atlas->current_row_x,
        .y = (kan_instance_offset_t) atlas->current_row_y,
        .z = 0u,
        .width = glyph_width,
        .height = glyph_height,
        .depth = 1u,
    };

    kan_render_image_upload_data_region (atlas->image, (kan_instance_size_t) atlas->current_layer, 0u, region,
                                         glyph_width * glyph_height, slot->bitmap.buffer);

    rendered->layer = atlas->current_layer;
    rendered->bitmap_bearing.x = TO_26_6 ((int32_t) slot->bitmap_left);
    rendered->bitmap_bearing.y = TO_26_6 ((int32_t) slot->bitmap_top);
    rendered->bitmap_size.x = TO_26_6 ((int32_t) glyph_width);
    rendered->bitmap_size.y = TO_26_6 ((int32_t) glyph_height);
    rendered->uv_min.x = (float) atlas->current_row_x / (float) atlas_width;
    rendered->uv_min.y = (float) atlas->current_row_y / (float) atlas_height;
    rendered->uv_max.x = ((float) atlas->current_row_x + (float) glyph_width) / (float) atlas_width;
    rendered->uv_max.y = ((float) atlas->current_row_y + (float) glyph_height) / (float) atlas_height;

    // Update cursor. Overflows will be handled during next glyph render.
    atlas->current_row_x += glyph_width + KAN_TEXT_FT_HB_SDF_ATLAS_GLYPH_BORDER;
    atlas->current_row_max_height = KAN_MAX (atlas->current_row_max_height, glyph_height);
}

static void shape_text_node_utf8 (struct shape_context_t *context, struct text_node_t *node)
{
    // TODO: For the future (after we get implementation kind of working):
    //       harfbuzz fonts and icu break iterators should be cached as their creation seems to be very costly.

    KAN_CPU_SCOPED_STATIC_SECTION (kan_font_library_shape_utf8)
    const hb_direction_t horizontal_direction = hb_script_get_horizontal_direction (node->utf8.script);
    const bool can_break = shape_is_break_allowed (context->request->reading_direction, horizontal_direction);

    // We only need to calculate breaks if we can actually break the segment.
    if (can_break)
    {
        // We do not generally expect icu functions to randomly fail, so we check the status only in asserts.
        UErrorCode icu_status = U_ZERO_ERROR;
        UBreakIterator *icu_break_iterator = ubrk_open (UBRK_LINE, NULL, NULL, 0u, &icu_status);

        KAN_ASSERT (icu_status <= U_ZERO_ERROR)
        CUSHION_DEFER { ubrk_close (icu_break_iterator); }

        {
            UText icu_text = UTEXT_INITIALIZER;
            utext_openUTF8 (&icu_text, node->utf8.data, node->utf8.length, &icu_status);
            KAN_ASSERT (icu_status <= U_ZERO_ERROR)

            ubrk_setUText (icu_break_iterator, &icu_text, &icu_status);
            utext_close (&icu_text);
            KAN_ASSERT (icu_status <= U_ZERO_ERROR)
        }

        while (true)
        {
            const int32_t next_break_index = ubrk_next (icu_break_iterator);
            if (next_break_index == UBRK_DONE)
            {
                break;
            }

            struct icu_break_t *item = kan_dynamic_array_add_last (&context->icu_breaks);
            if (!item)
            {
                kan_dynamic_array_set_capacity (&context->icu_breaks, context->icu_breaks.size * 2u);
                item = kan_dynamic_array_add_last (&context->icu_breaks);
            }

            item->index = (uint32_t) next_break_index;
            const int32_t icu_tag = ubrk_getRuleStatus (icu_break_iterator);
            item->hard = icu_tag >= UBRK_LINE_HARD && icu_tag < UBRK_LINE_HARD_LIMIT;
        }
    }

    hb_direction_t harfbuzz_direction = HB_DIRECTION_LTR;
    switch (context->request->orientation)
    {
    case KAN_TEXT_ORIENTATION_HORIZONTAL:
        harfbuzz_direction = horizontal_direction;
        break;

    case KAN_TEXT_ORIENTATION_VERTICAL:
        harfbuzz_direction = HB_DIRECTION_TTB;
        break;
    }

    hb_buffer_set_script (context->harfbuzz_buffer, node->utf8.script);
    hb_buffer_set_direction (context->harfbuzz_buffer, harfbuzz_direction);
    hb_buffer_set_cluster_level (context->harfbuzz_buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
    hb_buffer_add_utf8 (context->harfbuzz_buffer, node->utf8.data, -1, 0u, -1);
    hb_shape (context->harfbuzz_font, context->harfbuzz_buffer, NULL, 0u);
    CUSHION_DEFER { hb_buffer_clear_contents (context->harfbuzz_buffer); }

    // TODO: When analyzing shaping cost later: it is much faster in release mode.

    unsigned int glyph_count;
    const hb_glyph_info_t *glyph_infos = hb_buffer_get_glyph_infos (context->harfbuzz_buffer, &glyph_count);
    const hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions (context->harfbuzz_buffer, &glyph_count);

    struct hb_font_extents_t font_extents;
    hb_font_get_extents_for_direction (context->harfbuzz_font, harfbuzz_direction, &font_extents);
    const int32_t line_space_26_6 = font_extents.ascender - font_extents.descender + font_extents.line_gap;

    struct shape_sequence_t *last_sequence = NULL;
#define NEW_SEQUENCE                                                                                                   \
    last_sequence = kan_dynamic_array_add_last (&context->sequences);                                                  \
    if (!last_sequence)                                                                                                \
    {                                                                                                                  \
        kan_dynamic_array_set_capacity (&context->sequences, context->sequences.size * 2u);                            \
        last_sequence = kan_dynamic_array_add_last (&context->sequences);                                              \
    }                                                                                                                  \
                                                                                                                       \
    last_sequence->first_glyph_index = context->output->glyphs.size;                                                   \
    last_sequence->first_icon_index = context->output->icons.size;                                                     \
    last_sequence->length_26_6 = 0;                                                                                    \
    last_sequence->biggest_line_space_26_6 = line_space_26_6

    if (context->sequences.size > 0u)
    {
        last_sequence = &((struct shape_sequence_t *) context->sequences.data)[context->sequences.size - 1u];
        last_sequence->biggest_line_space_26_6 = KAN_MAX (last_sequence->biggest_line_space_26_6, line_space_26_6);
    }
    else
    {
        NEW_SEQUENCE;
    }

    // Remark: below we approximate sequence lengths through advances, which might not be correct in all cases,
    // but should be good enough for the most cases in most fonts. And it makes estimation easier as well.

    // As harfbuzz always orders output in left-to-right top-to-bottom order, right-to-left and bottom-to-top
    // sequences must be processed in reversed order to break the sequences correctly. However, we don't have
    // bottom-to-top support for now, therefore it is only right-to-left.
    const bool forward_string_processing =
        context->request->orientation == KAN_TEXT_ORIENTATION_VERTICAL ||
        context->request->reading_direction == KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT;

    const struct icu_break_t *breaks = (struct icu_break_t *) context->icu_breaks.data;
    const struct icu_break_t *breaks_end = breaks ? breaks + context->icu_breaks.size : NULL;

    if (can_break)
    {
        // Forward-ordered breaking processing.
        kan_instance_size_t next_unprocessed_glyph_index =
            forward_string_processing ? 0u : ((kan_instance_size_t) glyph_count) - 1u;
        // We do not need to reverse ICU breaks in reversed processing, only harfbuzz glyphs.
        const struct icu_break_t *next_break = breaks;

        // Will underflow to max value in reversed processing, and it is by design.
        while (next_unprocessed_glyph_index < glyph_count)
        {
            // Grab as many glyphs as we can properly fit into the sequence until line break.
            kan_instance_size_t grab_limit = context->primary_axis_limit_26_6 - last_sequence->length_26_6;
            kan_instance_size_t grab_until = next_unprocessed_glyph_index;
            kan_instance_size_t current_grabbed_length_26_6 = 0u;
            const struct icu_break_t *encountered_break = NULL;

            // Will underflow to max value in reversed processing, and it is by design.
            while (grab_until < glyph_count)
            {
                if (next_break < breaks_end && glyph_infos[grab_until].cluster >= next_break->index)
                {
                    // We've got to the next break point.
                    encountered_break = next_break;
                    ++next_break;
                    break;
                }

                kan_instance_size_t advance = 0u;
                switch (context->request->orientation)
                {
                case KAN_TEXT_ORIENTATION_HORIZONTAL:
                    advance = glyph_positions[grab_until].x_advance;
                    break;

                case KAN_TEXT_ORIENTATION_VERTICAL:
                    advance = glyph_positions[grab_until].y_advance;
                    break;
                }

                if (current_grabbed_length_26_6 + advance > grab_limit && last_sequence->length_26_6 != 0u)
                {
                    // Grabbed too much for the current line, start new sequence.
                    // We can just start new sequence like that as we're breaking grabs by line breaks.
                    NEW_SEQUENCE;
                    grab_limit = context->primary_axis_limit_26_6;
                }

                // We always need to grab first glyph and any overlay (zero advance) glyph on top of it.
                if (current_grabbed_length_26_6 + advance > grab_limit && grab_until != next_unprocessed_glyph_index &&
                    advance != 0u)
                {
                    // We cannot grab anymore and need unconventional break whatever the cost.
                    break;
                }

                current_grabbed_length_26_6 += advance;
                if (forward_string_processing)
                {
                    ++grab_until;
                }
                else
                {
                    --grab_until;
                }
            }

            if (forward_string_processing)
            {
                for (kan_loop_size_t index = next_unprocessed_glyph_index; index < grab_until; ++index)
                {
                    shape_append_to_sequence (context, last_sequence, true, glyph_infos + index,
                                              glyph_positions + index);
                }
            }
            else
            {
                // Use the same type for index to make underflow predictable.
                for (kan_instance_size_t index = next_unprocessed_glyph_index; index != grab_until; --index)
                {
                    shape_append_to_sequence (context, last_sequence, false, glyph_infos + index,
                                              glyph_positions + index);
                }
            }

            if (encountered_break && encountered_break->hard)
            {
                NEW_SEQUENCE;
            }

            next_unprocessed_glyph_index = grab_until;
        }
    }
    else
    {
        int32_t whole_length_26_6 = 0;
        for (kan_loop_size_t index = 0u; index < glyph_count; ++index)
        {
            switch (context->request->orientation)
            {
            case KAN_TEXT_ORIENTATION_HORIZONTAL:
                whole_length_26_6 += glyph_positions[index].x_advance;
                break;

            case KAN_TEXT_ORIENTATION_VERTICAL:
                whole_length_26_6 += glyph_positions[index].y_advance;
                break;
            }
        }

        if (last_sequence->length_26_6 + whole_length_26_6 > context->primary_axis_limit_26_6 &&
            // Corner case: sequence is already empty, no need for the new one.
            last_sequence->length_26_6 != 0u)
        {
            if (whole_length_26_6 > context->primary_axis_limit_26_6)
            {
                KAN_LOG (text, KAN_LOG_ERROR,
                         "Unbreakable sequence is longer than line/column limit (in 26.6 %lu > %lu. Will be visualized "
                         "as one line/column, but user code may clip it and will be right to do so.",
                         (unsigned long) whole_length_26_6, (unsigned long) context->primary_axis_limit_26_6)
            }

            // New sequence to avoid breaking.
            NEW_SEQUENCE;
        }

        if (forward_string_processing)
        {
            for (kan_loop_size_t index = 0u; index < glyph_count; ++index)
            {
                shape_append_to_sequence (context, last_sequence, true, glyph_infos + index, glyph_positions + index);
            }
        }
        else
        {
            for (kan_loop_size_t index = glyph_count - 1u; index != KAN_INT_MAX (kan_loop_size_t); --index)
            {
                shape_append_to_sequence (context, last_sequence, false, glyph_infos + index, glyph_positions + index);
            }
        }
    }

#undef NEW_SEQUENCE
    if (context->render_delayed.size > 0u)
    {
        KAN_CPU_SCOPED_STATIC_SECTION (kan_font_library_shape_glyph_render)
        KAN_ATOMIC_INT_SCOPED_LOCK (&context->library->freetype_lock)

        switch (context->request->render_format)
        {
        case KAN_FONT_GLYPH_RENDER_FORMAT_SDF:
            FT_Set_Char_Size (context->current_category->freetype_face, TO_26_6 (KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE),
                              TO_26_6 (KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE), 0u, 0u);
            break;
        }

        for (kan_loop_size_t index = 0u; index < context->render_delayed.size; ++index)
        {
            struct shape_render_delayed_reminder_t *render_delayed =
                &((struct shape_render_delayed_reminder_t *) context->render_delayed.data)[index];

            struct kan_text_shaped_glyph_instance_data_t *glyph =
                &((struct kan_text_shaped_glyph_instance_data_t *)
                      context->output->glyphs.data)[render_delayed->shaped_glyph_index];

            // Check if anyone else already rendered this glyph while we were shaping other glyphs.
            if (shape_extract_render_data_for_glyph_concurrently (context, render_delayed->font_glyph_index, glyph))
            {
                continue;
            }

            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&context->current_category->glyphs_read_write_lock)
            // Now check again under write access.

            struct font_glyph_node_t *glyph_node =
                font_library_category_find_glyph_unsafe (context->current_category, render_delayed->font_glyph_index);
            struct font_rendered_glyph_node_t *rendered = NULL;

            if (glyph_node)
            {
                rendered = font_glyph_node_find_rendered (glyph_node, context->request->render_format);
                if (rendered)
                {
                    shape_apply_rendered_data_to_glyph (context, glyph, rendered);
                    continue;
                }
            }

            if (!glyph_node)
            {
                KAN_ATOMIC_INT_SCOPED_LOCK (&context->library->allocator_lock)
                glyph_node =
                    kan_stack_group_allocator_allocate (&context->library->allocator, sizeof (struct font_glyph_node_t),
                                                        alignof (struct font_glyph_node_t));

                glyph_node->node.hash = (kan_hash_t) render_delayed->font_glyph_index;
                glyph_node->glyph_index = render_delayed->font_glyph_index;
                glyph_node->rendered_first = NULL;

                kan_hash_storage_add (&context->current_category->glyphs, &glyph_node->node);
                kan_hash_storage_update_bucket_count_default (&context->current_category->glyphs,
                                                              KAN_TEXT_FT_HB_FONT_LIBRARY_BUCKETS);
            }

            {
                KAN_ATOMIC_INT_SCOPED_LOCK (&context->library->allocator_lock)
                rendered = kan_stack_group_allocator_allocate (&context->library->allocator,
                                                               sizeof (struct font_rendered_glyph_node_t),
                                                               alignof (struct font_rendered_glyph_node_t));
            }

            rendered->next = glyph_node->rendered_first;
            glyph_node->rendered_first = rendered;
            rendered->format = context->request->render_format;

            rendered->layer = 0u;
            rendered->bitmap_bearing.x = 0;
            rendered->bitmap_bearing.y = 0;
            rendered->bitmap_size.x = 0;
            rendered->bitmap_size.y = 0;
            rendered->uv_min.x = 0.0f;
            rendered->uv_min.y = 0.0f;
            rendered->uv_max.x = 0.0f;
            rendered->uv_max.y = 0.0f;

            // TODO: For vertical layout, we need separate rendered glyph as freetype says
            //       vertical needs other loader flag and might have significant differences.
            FT_Error freetype_error = FT_Load_Glyph (context->current_category->freetype_face,
                                                     (FT_UInt) render_delayed->font_glyph_index, FT_LOAD_DEFAULT);

            if (freetype_error)
            {
                KAN_LOG (text, KAN_LOG_ERROR, "Failed to load glyph at index %lu for rendering.\n",
                         (unsigned long) render_delayed->font_glyph_index)
                continue;
            }

            switch (rendered->format)
            {
            case KAN_FONT_GLYPH_RENDER_FORMAT_SDF:
                shape_render_sdf (context, rendered, render_delayed->font_glyph_index);
                break;
            }

            shape_apply_rendered_data_to_glyph (context, glyph, rendered);
        }
    }

    // TODO: We might want to rethink the whole initial locale-language approach.
    //       1. ICU breaks work without directly specifying locale.
    //       2. Harfbuzz languages can change things, but they're not as influential as scripts.
    //       3. Text horizontal and vertical direction should be uniformly selected during shaping and only drop-in
    //          bidi phrases should be allowed, otherwise unsolvable questions like the question above will arise.
    //       All that means that our current system is "too flexible" and makes it easy to build erroneous combinations.
    //       I think we should "float" into other direction:
    //       1. Text is a combination of utf8 segments and injections (icons, styles, marks).
    //       2. There is nothing explicit about languages and scripts in text description.
    //       3. Scripts are automatically detected from codepoints during text build, and segmentation is done.
    //          Languages are omitted for now, we might inject them later if we really need them.
    //       4. Font libraries use script ids and styles to select categories. Let the higher-end deal with remapping
    //          higher-end structures to script ids.
    //       5. Line horizontal direction is specified as shaping parameter. Other directions are taken from scripts.
    //       6. Line horizontal direction (when in horizontal orientation) decides where logical line always starts for
    //          any script, therefore kind-of-solving question above. And locale will dictate it. We might even want to
    //          disable line breaks for non-line-horizontal-directions, but it is up for consideration.
    //       7. Orientation is a shaping parameter too. With verticals, lines are columns.
    //       8. Do not forget about left-aligned, right-aligned and centered lines/columns.
    //       9. We should not need locale-language data pair on universe_locale level anymore if this works out.
}

static void shape_text_node_icon (struct shape_context_t *context, struct text_node_t *node)
{
    hb_direction_t harfbuzz_direction = HB_DIRECTION_LTR;
    switch (context->request->orientation)
    {
    case KAN_TEXT_ORIENTATION_HORIZONTAL:
        switch (context->request->reading_direction)
        {
        case KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT:
            harfbuzz_direction = HB_DIRECTION_LTR;
            break;

        case KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT:
            harfbuzz_direction = HB_DIRECTION_RTL;
            break;
        }

        break;

    case KAN_TEXT_ORIENTATION_VERTICAL:
        harfbuzz_direction = HB_DIRECTION_TTB;
        break;
    }

    // TODO: FIX IT LATER. HARFBUZZ EXPECTS GLYPH INDEX NOT UNICODE CODEPOINT HERE!

    hb_glyph_extents_t extents;
    if (!hb_font_get_glyph_extents_for_origin (context->harfbuzz_font, node->icon.base_codepoint, harfbuzz_direction,
                                               &extents))
    {
        KAN_LOG (text, KAN_LOG_ERROR,
                 "Cannot add icon as codepoint as it was not possible to query extents of codepoint %lu.",
                 (unsigned long) node->icon.base_codepoint)
        return;
    }

    const int32_t scaled_x_bearing_26_6 = (int32_t) roundf ((float) extents.x_bearing * node->icon.x_scale);
    const int32_t scaled_y_bearing_26_6 = (int32_t) roundf ((float) extents.y_bearing * node->icon.y_scale);
    const int32_t scaled_width_26_6 = (int32_t) roundf ((float) extents.width * node->icon.x_scale);
    const int32_t scaled_height_26_6 = (int32_t) roundf ((float) extents.height * node->icon.y_scale);
    const int32_t length_26_6 =
        context->request->orientation == KAN_TEXT_ORIENTATION_HORIZONTAL ? scaled_width_26_6 : scaled_height_26_6;

    struct shape_sequence_t *last_sequence = NULL;
    if (context->sequences.size > 0u)
    {
        last_sequence = &((struct shape_sequence_t *) context->sequences.data)[context->sequences.size - 1u];
    }

    if (!last_sequence || last_sequence->length_26_6 + length_26_6)
    {
        last_sequence = kan_dynamic_array_add_last (&context->sequences);
        if (!last_sequence)
        {
            kan_dynamic_array_set_capacity (&context->sequences, context->sequences.size * 2u);
            last_sequence = kan_dynamic_array_add_last (&context->sequences);
        }

        last_sequence->first_glyph_index = context->output->glyphs.size;
        last_sequence->first_icon_index = context->output->icons.size;
        last_sequence->length_26_6 = 0;

        struct hb_font_extents_t font_extents;
        hb_font_get_extents_for_direction (context->harfbuzz_font, harfbuzz_direction, &font_extents);
        const int32_t line_space_26_6 = font_extents.ascender - font_extents.descender + font_extents.line_gap;
        last_sequence->biggest_line_space_26_6 = line_space_26_6;
    }

    struct kan_text_shaped_icon_instance_data_t *shaped = kan_dynamic_array_add_last (&context->output->icons);
    if (!shaped)
    {
        kan_dynamic_array_set_capacity (&context->output->icons, context->output->icons.size * 2u);
        shaped = kan_dynamic_array_add_last (&context->output->icons);
    }

    shaped->icon_index = node->icon.icon_index;
    int32_t origin_x = 0u;
    int32_t origin_y = 0u;

    switch (context->request->orientation)
    {
    case KAN_TEXT_ORIENTATION_HORIZONTAL:
        if (context->request->reading_direction == KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT)
        {
            origin_x = last_sequence->length_26_6;
            origin_y = 0;
        }
        else
        {
            origin_x = context->primary_axis_limit_26_6 - last_sequence->length_26_6 - scaled_width_26_6 -
                       scaled_x_bearing_26_6;
            origin_y = 0;
        }

        last_sequence->length_26_6 += length_26_6;
        break;

    case KAN_TEXT_ORIENTATION_VERTICAL:
        origin_x = 0;
        origin_y = last_sequence->length_26_6;
        last_sequence->length_26_6 += length_26_6;
        break;
    }

    struct kan_int32_vector_2_t *min_26_6 = (struct kan_int32_vector_2_t *) &shaped->min;
    struct kan_int32_vector_2_t *max_26_6 = (struct kan_int32_vector_2_t *) &shaped->max;
    min_26_6->x = origin_x + scaled_x_bearing_26_6;
    min_26_6->y = origin_y + scaled_y_bearing_26_6;
    max_26_6->x = origin_x + scaled_x_bearing_26_6 + scaled_width_26_6;
    max_26_6->y = origin_y + scaled_y_bearing_26_6 + scaled_height_26_6;
}

static void shape_post_process_sequences (struct shape_context_t *context)
{
    context->output->min.x = 0;
    context->output->min.y = 0;
    context->output->max.x = 0;
    context->output->max.y = 0;
    int32_t baseline_26_6 = 0;

    for (kan_loop_size_t sequence_index = 0u; sequence_index < context->sequences.size; ++sequence_index)
    {
        const struct shape_sequence_t *sequence =
            &((struct shape_sequence_t *) context->sequences.data)[sequence_index];
        int32_t alignment_offset_26_6 = 0;

        if (context->request->reading_direction == KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT ||
            context->request->orientation == KAN_TEXT_ORIENTATION_VERTICAL)
        {
            switch (context->request->alignment)
            {
            case KAN_TEXT_SHAPING_ALIGNMENT_LEFT:
                // No alignment offset.
                break;

            case KAN_TEXT_SHAPING_ALIGNMENT_CENTER:
                alignment_offset_26_6 = (context->primary_axis_limit_26_6 - sequence->length_26_6) / 2;
                break;

            case KAN_TEXT_SHAPING_ALIGNMENT_RIGHT:
                alignment_offset_26_6 = context->primary_axis_limit_26_6 - sequence->length_26_6;
                break;
            }
        }
        else
        {
            switch (context->request->alignment)
            {
            case KAN_TEXT_SHAPING_ALIGNMENT_LEFT:
                alignment_offset_26_6 = sequence->length_26_6 - context->primary_axis_limit_26_6;
                break;

            case KAN_TEXT_SHAPING_ALIGNMENT_CENTER:
                alignment_offset_26_6 = (sequence->length_26_6 - context->primary_axis_limit_26_6) / 2;
                break;

            case KAN_TEXT_SHAPING_ALIGNMENT_RIGHT:
                // No alignment offset.
                break;
            }
        }

        kan_loop_size_t glyph_limit = context->output->glyphs.size;
        kan_loop_size_t icon_limit = context->output->icons.size;

        if (sequence_index + 1u < context->sequences.size)
        {
            const struct shape_sequence_t *next_sequence =
                &((struct shape_sequence_t *) context->sequences.data)[sequence_index + 1u];

            glyph_limit = next_sequence->first_glyph_index;
            icon_limit = next_sequence->first_icon_index;
        }

        for (kan_loop_size_t glyph_index = sequence->first_glyph_index; glyph_index < glyph_limit; ++glyph_index)
        {
            struct kan_text_shaped_glyph_instance_data_t *glyph =
                &((struct kan_text_shaped_glyph_instance_data_t *) context->output->glyphs.data)[glyph_index];
            struct kan_int32_vector_2_t *min_26_6 = (struct kan_int32_vector_2_t *) &glyph->min;
            struct kan_int32_vector_2_t *max_26_6 = (struct kan_int32_vector_2_t *) &glyph->max;

            switch (context->request->orientation)
            {
            case KAN_TEXT_ORIENTATION_HORIZONTAL:
                min_26_6->x += alignment_offset_26_6;
                min_26_6->y += baseline_26_6;
                max_26_6->x += alignment_offset_26_6;
                max_26_6->y += baseline_26_6;
                break;

            case KAN_TEXT_ORIENTATION_VERTICAL:
                min_26_6->x += baseline_26_6;
                min_26_6->y += alignment_offset_26_6;
                max_26_6->x += baseline_26_6;
                max_26_6->y += alignment_offset_26_6;
                break;
            }

            glyph->min.x = FROM_26_6 (min_26_6->x);
            glyph->min.y = FROM_26_6 (min_26_6->y);
            glyph->max.x = FROM_26_6 (max_26_6->x);
            glyph->max.y = FROM_26_6 (max_26_6->y);

            context->output->min.x = KAN_MIN (context->output->min.x, (int32_t) glyph->min.x);
            context->output->min.y = KAN_MIN (context->output->min.y, (int32_t) glyph->min.y);
            context->output->max.x = KAN_MAX (context->output->max.x, (int32_t) glyph->max.x);
            context->output->max.y = KAN_MAX (context->output->max.y, (int32_t) glyph->max.y);
        }

        for (kan_loop_size_t icon_index = sequence->first_icon_index; icon_index < icon_limit; ++icon_index)
        {
            struct kan_text_shaped_icon_instance_data_t *icon =
                &((struct kan_text_shaped_icon_instance_data_t *) context->output->icons.data)[icon_index];
            struct kan_int32_vector_2_t *min_26_6 = (struct kan_int32_vector_2_t *) &icon->min;
            struct kan_int32_vector_2_t *max_26_6 = (struct kan_int32_vector_2_t *) &icon->max;

            switch (context->request->orientation)
            {
            case KAN_TEXT_ORIENTATION_HORIZONTAL:
                min_26_6->x += alignment_offset_26_6;
                min_26_6->y += baseline_26_6;
                max_26_6->x += alignment_offset_26_6;
                max_26_6->y += baseline_26_6;
                break;

            case KAN_TEXT_ORIENTATION_VERTICAL:
                min_26_6->x += baseline_26_6;
                min_26_6->y += alignment_offset_26_6;
                max_26_6->x += baseline_26_6;
                max_26_6->y += alignment_offset_26_6;
                break;
            }

            icon->min.x = roundf (FROM_26_6 (min_26_6->x));
            icon->min.y = roundf (FROM_26_6 (min_26_6->y));
            icon->max.x = roundf (FROM_26_6 (max_26_6->x));
            icon->max.y = roundf (FROM_26_6 (max_26_6->y));

            context->output->min.x = KAN_MIN (context->output->min.x, (int32_t) icon->min.x);
            context->output->min.y = KAN_MIN (context->output->min.y, (int32_t) icon->min.y);
            context->output->max.x = KAN_MAX (context->output->max.x, (int32_t) icon->max.x);
            context->output->max.y = KAN_MAX (context->output->max.y, (int32_t) icon->max.y);
        }

        baseline_26_6 += sequence->biggest_line_space_26_6;
    }
}

static void shape_context_shutdown (struct shape_context_t *context)
{
    shape_reset_category (context);
    hb_buffer_destroy (context->harfbuzz_buffer);
    kan_dynamic_array_shutdown (&context->icu_breaks);
    kan_dynamic_array_shutdown (&context->sequences);
    kan_dynamic_array_shutdown (&context->render_delayed);
}

bool kan_font_library_shape (kan_font_library_t instance,
                             struct kan_text_shaping_request_t *request,
                             struct kan_text_shaped_data_t *output)
{
    // TODO: Optimize this later, apply necessary caching.

    KAN_CPU_SCOPED_STATIC_SECTION (kan_font_library_shape)
    struct shape_context_t context = {
        .library = KAN_HANDLE_GET (instance),
        .request = request,
        .output = output,

        .script = HB_SCRIPT_COMMON,
        .style = NULL,
        .mark_index = 0u,
        .primary_axis_limit_26_6 = (int32_t) TO_26_6 (request->primary_axis_limit),

        .current_category = NULL,
        .harfbuzz_font = NULL,
        .harfbuzz_buffer = hb_buffer_create (),
    };

    kan_dynamic_array_init (&context.icu_breaks, KAN_TEXT_FT_HB_FONT_SHAPE_LINE_BREAKS_INITIAL,
                            sizeof (struct icu_break_t), alignof (struct icu_break_t),
                            shaping_temporary_allocation_group);

    kan_dynamic_array_init (&context.sequences, KAN_TEXT_FT_HB_FONT_SHAPE_SEQUENCES_INITIAL,
                            sizeof (struct shape_sequence_t), alignof (struct shape_sequence_t),
                            shaping_temporary_allocation_group);

    kan_dynamic_array_init (&context.render_delayed, KAN_TEXT_FT_HB_FONT_SHAPE_DELAYED_RENDER_BASE,
                            sizeof (struct shape_render_delayed_reminder_t),
                            alignof (struct shape_render_delayed_reminder_t), shaping_temporary_allocation_group);

    CUSHION_DEFER { shape_context_shutdown (&context); }
    kan_dynamic_array_set_capacity (&output->glyphs, KAN_TEXT_FT_HB_FONT_SHAPED_GLYPHS_INITIAL);
    struct text_node_t *text_node = KAN_HANDLE_GET (request->text);

    while (text_node)
    {
        switch (text_node->type)
        {
        case TEXT_NODE_TYPE_UTF8:
            if (context.script != text_node->utf8.script)
            {
                shape_reset_category (&context);
            }

            context.script = text_node->utf8.script;
            if (!shape_choose_category (&context))
            {
                return false;
            }

            shape_text_node_utf8 (&context, text_node);
            context.icu_breaks.size = 0u;
            context.render_delayed.size = 0u;
            break;

        case TEXT_NODE_TYPE_ICON:
            if (context.script != HB_SCRIPT_COMMON)
            {
                shape_reset_category (&context);
            }

            context.script = HB_SCRIPT_COMMON;
            if (!shape_choose_category (&context))
            {
                return false;
            }

            shape_text_node_icon (&context, text_node);
            break;

        case TEXT_NODE_TYPE_STYLE:
            if (context.style != text_node->style.style)
            {
                shape_reset_category (&context);
            }

            context.style = text_node->style.style;
            context.mark_index = text_node->style.mark_index;
            break;
        }

        text_node = text_node->next;
    }

    shape_post_process_sequences (&context);
    kan_dynamic_array_set_capacity (&output->glyphs, output->glyphs.size);
    return true;
}

void kan_font_library_destroy (kan_font_library_t instance)
{
    struct font_library_t *library = KAN_HANDLE_GET (instance);
    for (kan_loop_size_t index = 0u; index < library->categories_count; ++index)
    {
        struct font_library_category_t *category = &library->categories[index];
        if (category->harfbuzz_face)
        {
            hb_face_destroy (category->harfbuzz_face);
        }

        if (category->harfbuzz_face_blob)
        {
            hb_blob_destroy (category->harfbuzz_face_blob);
        }

        if (category->freetype_face)
        {
            FT_Done_Face (category->freetype_face);
        }

        // Glyph nodes are allocated through stack group, so we don't need to deallocate them manually.
        kan_hash_storage_shutdown (&category->glyphs);
    }

    kan_render_image_destroy (library->sdf_atlas.image);
    FT_Done_Library (library->freetype_library);
    kan_stack_group_allocator_shutdown (&library->allocator);

    const kan_memory_size_t library_size =
        sizeof (struct font_library_t) + sizeof (struct font_library_category_t) * library->categories_count;
    kan_free_general (font_library_allocation_group, library, library_size);
}
