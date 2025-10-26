if (NOT ICU_FOUND)
    return ()
endif ()

register_concrete (text_ft_hb)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE 
        ABSTRACT cpu_profiler error hash log memory threading
        CONCRETE_INTERFACE container 
        THIRD_PARTY freetype harfbuzz ICU::uc)
setup_core_preprocessing ()
concrete_implements_abstract (text)

set (KAN_TEXT_FT_HB_SDF_ATLAS_WIDTH "2048" CACHE STRING "Width for SDF atlas image.")
set (KAN_TEXT_FT_HB_SDF_ATLAS_HEIGHT "2048" CACHE STRING "Height for SDF atlas image.")
set (KAN_TEXT_FT_HB_SDF_ATLAS_LAYERS "1" CACHE STRING "Initial count of layers for SDF atlas image.")
set (KAN_TEXT_FT_HB_SDF_ATLAS_GLYPH_BORDER "1" CACHE STRING 
        "Size of an empty border space between SDF glyphs in atlas.")
set (KAN_TEXT_FT_HB_SDF_ATLAS_LAYER_STEP "1" CACHE STRING
        "Count of layers to add to SDF atlas when there is no space left to upload new glyph.")
set (KAN_TEXT_FT_HB_FONT_LIBRARY_STACK "65536" CACHE STRING "Size of font library data stack group allocator page.")
set (KAN_TEXT_FT_HB_FONT_LIBRARY_BUCKETS "269" CACHE STRING 
        "Initial count of buckets for glyphs inside font library category.")
set (KAN_TEXT_FT_HB_FONT_SHAPE_LINE_BREAKS_INITIAL "64" CACHE STRING
        "Initial capacity of line breaks array for shaping.")
set (KAN_TEXT_FT_HB_FONT_SHAPE_SEQUENCES_INITIAL "16" CACHE STRING
        "Initial capacity of sequences (lines or columns) meta temporary array for shaping.")
set (KAN_TEXT_FT_HB_FONT_SHAPE_DELAYED_RENDER_BASE "16" CACHE STRING
        "Base size for an array of glyphs that are not rendered and should be rendered later during shaping.")
set (KAN_TEXT_FT_HB_FONT_SHAPED_GLYPHS_INITIAL "64" CACHE STRING
        "Initial capacity for shaped glyphs array in shaped data.")
set (KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE "24" CACHE STRING "Font size for rendering glyphs in SDF format.")

concrete_compile_definitions (
        PRIVATE
        KAN_TEXT_FT_HB_SDF_ATLAS_WIDTH=${KAN_TEXT_FT_HB_SDF_ATLAS_WIDTH}
        KAN_TEXT_FT_HB_SDF_ATLAS_HEIGHT=${KAN_TEXT_FT_HB_SDF_ATLAS_HEIGHT}
        KAN_TEXT_FT_HB_SDF_ATLAS_LAYERS=${KAN_TEXT_FT_HB_SDF_ATLAS_LAYERS}
        KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE=${KAN_TEXT_FT_HB_SDF_ATLAS_FONT_SIZE}
        KAN_TEXT_FT_HB_SDF_ATLAS_GLYPH_BORDER=${KAN_TEXT_FT_HB_SDF_ATLAS_GLYPH_BORDER}
        KAN_TEXT_FT_HB_SDF_ATLAS_LAYER_STEP=${KAN_TEXT_FT_HB_SDF_ATLAS_LAYER_STEP}
        KAN_TEXT_FT_HB_FONT_LIBRARY_STACK=${KAN_TEXT_FT_HB_FONT_LIBRARY_STACK}
        KAN_TEXT_FT_HB_FONT_LIBRARY_BUCKETS=${KAN_TEXT_FT_HB_FONT_LIBRARY_BUCKETS}
        KAN_TEXT_FT_HB_FONT_SHAPE_LINE_BREAKS_INITIAL=${KAN_TEXT_FT_HB_FONT_SHAPE_LINE_BREAKS_INITIAL}
        KAN_TEXT_FT_HB_FONT_SHAPE_SEQUENCES_INITIAL=${KAN_TEXT_FT_HB_FONT_SHAPE_SEQUENCES_INITIAL}
        KAN_TEXT_FT_HB_FONT_SHAPE_DELAYED_RENDER_BASE=${KAN_TEXT_FT_HB_FONT_SHAPE_DELAYED_RENDER_BASE}
        KAN_TEXT_FT_HB_FONT_SHAPED_GLYPHS_INITIAL=${KAN_TEXT_FT_HB_FONT_SHAPED_GLYPHS_INITIAL})

option (KAN_TEXT_FT_HB_PROFILE_MEMORY "Whether memory profiling is enabled for freetype and harfbuzz." ON)
if (KAN_TEXT_FT_HB_PROFILE_MEMORY)
    concrete_compile_definitions (PRIVATE KAN_TEXT_FT_HB_PROFILE_MEMORY)
endif ()
