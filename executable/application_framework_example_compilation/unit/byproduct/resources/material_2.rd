//! material_t

shader = shader_object_1

+passes {
    name = "visible_world"
    +options {
        name = "test_option"
        value = 3
    }
}

+passes {
    name = "shadow"
    +options {
        name = "other_test_option"
        value = 2
    }
}
