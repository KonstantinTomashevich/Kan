//! material_t

shader_sources = "shader_source_2.rpl", "shader_source_3.rpl"

+passes {
    name = "visible_world"
    +options {
        name = "test_option"
        value = 1
    }
}

+passes {
    name = "shadow"
    +options {
        name = "other_test_option"
        value = 3
    }
}
