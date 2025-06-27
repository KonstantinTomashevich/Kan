#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <stdlib.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/error/critical.h>
#include <kan/image/image.h>
#include <kan/memory/allocation.h>

#if defined(KAN_IMAGE_STB_PROFILE_MEMORY)
static bool statics_initialized = false;
static kan_allocation_group_t allocation_group_image;
static kan_allocation_group_t allocation_group_stb;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group_image = kan_allocation_group_get_child (kan_allocation_group_root (), "image");
        allocation_group_stb = kan_allocation_group_get_child (allocation_group_image, "stb");
        statics_initialized = true;
    }
}

void *stb_malloc (size_t size)
{
    ensure_statics_initialized ();
    const kan_memory_size_t allocation_size =
        kan_apply_alignment (size + sizeof (kan_memory_size_t), _Alignof (kan_memory_size_t));
    kan_memory_size_t *data =
        kan_allocate_general (allocation_group_stb, allocation_size, _Alignof (kan_memory_size_t));
    *data = allocation_size;
    return data + 1u;
}

void stb_free (void *pointer)
{
    if (pointer)
    {
        kan_memory_size_t *allocation = ((kan_memory_size_t *) pointer) - 1u;
        kan_free_general (allocation_group_stb, allocation, *allocation);
    }
}

void *stb_realloc (void *pointer, size_t new_size)
{
    if (!pointer)
    {
        return stb_malloc (new_size);
    }
    else if (new_size == 0u)
    {
        stb_free (pointer);
        return NULL;
    }

    void *new_allocated_data = stb_malloc (new_size);
    const kan_memory_size_t old_size = *(((kan_memory_size_t *) pointer) - 1u);
    memcpy (new_allocated_data, pointer, KAN_MIN (old_size - sizeof (kan_memory_size_t), new_size));
    stb_free (pointer);
    return new_allocated_data;
}

#    define STBI_MALLOC stb_malloc
#    define STBI_REALLOC stb_realloc
#    define STBI_FREE stb_free

#    define STBIW_MALLOC stb_malloc
#    define STBIW_REALLOC stb_realloc
#    define STBIW_FREE stb_free
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(X) KAN_ASSERT (X)
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(X) KAN_ASSERT (X)
#define STBIW_WINDOWS_UTF8
#include <stb_image_write.h>

static int stb_read (void *user_data, char *output_data, int size_to_read)
{
    struct kan_stream_t *stream = user_data;
    kan_file_size_t was_read = stream->operations->read (stream, (kan_file_size_t) size_to_read, output_data);
    return (int) was_read;
}

static void stb_skip (void *user_data, int delta)
{
    struct kan_stream_t *stream = user_data;
    stream->operations->seek (stream, KAN_STREAM_SEEK_CURRENT, (kan_file_offset_t) delta);
}

static int stb_eof (void *user_data)
{
    struct kan_stream_t *stream = user_data;
    char temporary;

    if (stream->operations->read (stream, 1u, &temporary) == 0u)
    {
        return 1;
    }
    else
    {
        stream->operations->seek (stream, KAN_STREAM_SEEK_CURRENT, -1);
        return 0;
    }
}

static stbi_io_callbacks read_io_callbacks = {
    .read = stb_read,
    .skip = stb_skip,
    .eof = stb_eof,
};

static void stb_write (void *user_data, void *data, int size)
{
    struct kan_stream_t *stream = user_data;
    stream->operations->write (stream, (kan_file_size_t) size, data);
}

void kan_image_raw_data_init (struct kan_image_raw_data_t *data)
{
    data->width = 0u;
    data->height = 0u;
    data->data = NULL;
}

void kan_image_raw_data_shutdown (struct kan_image_raw_data_t *data)
{
    if (data->data)
    {
        stbi_image_free (data->data);
    }
}

bool kan_image_load (struct kan_stream_t *stream, struct kan_image_raw_data_t *output)
{
    int width;
    int height;
    int components;
    unsigned char *data =
        stbi_load_from_callbacks (&read_io_callbacks, stream, &width, &height, &components, STBI_rgb_alpha);

    if (data)
    {
        KAN_ASSERT (components == 4u)
        output->width = (kan_instance_size_t) width;
        output->height = (kan_instance_size_t) height;
        output->data = data;
        return true;
    }

    return false;
}

bool kan_image_load_from_buffer (const void *buffer, kan_memory_size_t buffer_size, struct kan_image_raw_data_t *output)
{
    int width;
    int height;
    int components;
    unsigned char *data =
        stbi_load_from_memory (buffer, (int) buffer_size, &width, &height, &components, STBI_rgb_alpha);

    if (data)
    {
        KAN_ASSERT (components == 4u)
        output->width = (kan_instance_size_t) width;
        output->height = (kan_instance_size_t) height;
        output->data = data;
        return true;
    }

    return false;
}

bool kan_image_save (struct kan_stream_t *stream,
                     enum kan_image_save_format_t format,
                     struct kan_image_raw_data_t *input)
{
    switch (format)
    {
    case KAN_IMAGE_SAVE_FORMAT_PNG:
        return stbi_write_png_to_func (stb_write, stream, (int) input->width, (int) input->height, STBI_rgb_alpha,
                                       input->data, (int) input->width * STBI_rgb_alpha) > 0;

    case KAN_IMAGE_SAVE_FORMAT_TGA:
        return stbi_write_tga_to_func (stb_write, stream, (int) input->width, (int) input->height, STBI_rgb_alpha,
                                       input->data) > 0;

    case KAN_IMAGE_SAVE_FORMAT_BMP:
        return stbi_write_bmp_to_func (stb_write, stream, (int) input->width, (int) input->height, STBI_rgb_alpha,
                                       input->data) > 0;
    }

    KAN_ASSERT (false)
    return false;
}
