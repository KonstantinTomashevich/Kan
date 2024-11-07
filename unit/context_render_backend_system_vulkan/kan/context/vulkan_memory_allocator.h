#pragma once

#include <kan/api_common/mute_warnings.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>

// Order of includes matters here.
// clang-format off
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
// clang-format on

#define VMA_HEAVY_ASSERT(expr) KAN_ASSERT (expr)
#define VMA_DEDICATED_ALLOCATION 0
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 0
#define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY 256
#define VMA_USE_STL_SHARED_MUTEX 0
#define VMA_MEMORY_BUDGET 0
#define VMA_STATS_STRING_ENABLED 0
#define VMA_MAPPING_HYSTERESIS_ENABLED 0
#define VMA_KHR_MAINTENANCE5 0

#define VMA_VULKAN_VERSION 1001000 // Vulkan 1.1

#define VMA_DEBUG_LOG(...) KAN_LOG (vulkan_memory_allocator, KAN_LOG_DEBUG, __VA_ARGS__)

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <vk_mem_alloc.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END
