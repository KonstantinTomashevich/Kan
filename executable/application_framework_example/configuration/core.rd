//! kan_application_framework_core_configuration_t

plugins = ${PLUGINS}

${RESOURCE_DIRECTORIES}

${RESOURCE_PACKS}

environment_tags = ${ENVIRONMENT_TAGS}

root_world = "core_root"
plugin_directory_path = "${PLUGINS_DIRECTORY_PATH}"
world_directory_path = "${WORLDS_DIRECTORY_PATH}"
observe_world_definitions = ${OBSERVE_WORLD_DEFINITIONS}
world_definition_rescan_delay_ns = 100000000

enable_code_hot_reload = ${ENABLE_CODE_HOT_RELOAD}
code_hot_reload_delay_ns = 200000000
${AUTO_CODE_HOT_RELOAD_COMMAND}
