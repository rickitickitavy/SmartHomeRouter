idf_component_register(
    SRCS
        "src/ZigbeeCore.cpp"
        "src/ZigbeeEP.cpp"
        "src/ep/ZigbeeSwitch.cpp"
    INCLUDE_DIRS
        "include"
        "src"
    REQUIRES
        espressif__esp-zboss-lib
        espressif__esp-zigbee-lib
        freertos
        esp_timer
        esp_hw_support
        heap
        app_update
        driver
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_ZB_ENABLED=1)
target_include_directories(${COMPONENT_LIB} PUBLIC
    "${CMAKE_SOURCE_DIR}/include/idf_compat"
    "${CMAKE_SOURCE_DIR}/include"
)
