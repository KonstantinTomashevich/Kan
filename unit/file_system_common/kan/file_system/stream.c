#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

#include <kan/error/critical.h>
#include <kan/file_system/stream.h>
#include <kan/memory/allocation.h>

static kan_bool_t allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t allocation_group;

static kan_allocation_group_t get_allocation_group (void)
{
    if (!allocation_group_ready)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "file_stream");
        allocation_group_ready = KAN_TRUE;
    }

    return allocation_group;
}

struct file_stream_t
{
    struct kan_stream_t stream;
    FILE *file;
};

static uint64_t read (struct kan_stream_t *stream, uint64_t amount, void *output_buffer)
{
    return (uint64_t) fread (output_buffer, 1u, amount, ((struct file_stream_t *) stream)->file);
}

static uint64_t write (struct kan_stream_t *stream, uint64_t amount, const void *input_buffer)
{
    return (uint64_t) fwrite (input_buffer, 1u, amount, ((struct file_stream_t *) stream)->file);
}

static kan_bool_t flush (struct kan_stream_t *stream)
{
    return fflush (((struct file_stream_t *) stream)->file) == 0;
}

static uint64_t tell (struct kan_stream_t *stream)
{
    return (uint64_t) ftell (((struct file_stream_t *) stream)->file);
}

static kan_bool_t seek (struct kan_stream_t *stream, enum kan_stream_seek_pivot pivot, int64_t offset)
{
    switch (pivot)
    {
    case KAN_STREAM_SEEK_START:
        return fseek (((struct file_stream_t *) stream)->file, (long) offset, SEEK_SET) == 0;

    case KAN_STREAM_SEEK_CURRENT:
        return fseek (((struct file_stream_t *) stream)->file, (long) offset, SEEK_CUR) == 0;

    case KAN_STREAM_SEEK_END:
        return fseek (((struct file_stream_t *) stream)->file, (long) offset, SEEK_END) == 0;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static void close (struct kan_stream_t *stream)
{
    fclose (((struct file_stream_t *) stream)->file);
    kan_free_batched (get_allocation_group (), stream);
}

static struct kan_stream_operations_t direct_file_stream_read_operations = {
    .read = read,
    .write = NULL,
    .flush = NULL,
    .tell = tell,
    .seek = seek,
    .close = close,
};

static struct kan_stream_operations_t direct_file_stream_write_operations = {
    .read = NULL,
    .write = write,
    .flush = flush,
    .tell = tell,
    .seek = seek,
    .close = close,
};

struct kan_stream_t *kan_direct_file_stream_open_for_read (const char *path, kan_bool_t binary)
{
    FILE *file = fopen (path, binary ? "rb" : "r");
    if (file)
    {
        struct file_stream_t *stream = kan_allocate_batched (get_allocation_group (), sizeof (struct file_stream_t));
        stream->stream.operations = &direct_file_stream_read_operations;
        stream->file = file;
        return (struct kan_stream_t *) stream;
    }

    return NULL;
}

struct kan_stream_t *kan_direct_file_stream_open_for_write (const char *path, kan_bool_t binary)
{
    FILE *file = fopen (path, binary ? "wb" : "w");
    if (file)
    {
        struct file_stream_t *stream = kan_allocate_batched (get_allocation_group (), sizeof (struct file_stream_t));
        stream->stream.operations = &direct_file_stream_write_operations;
        stream->file = file;
        return (struct kan_stream_t *) stream;
    }

    return NULL;
}
