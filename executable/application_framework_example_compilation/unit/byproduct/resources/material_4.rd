//! material_t

shader = shader_object_2

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
