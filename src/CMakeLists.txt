# Role selected by ESP-IDF target: C6 = slave, S3/P4 = host.
if(CONFIG_IDF_TARGET_ESP32C6)
    set(ROLE_SRCS
        "idf_slave/app_main.cpp"
        "idf_slave/SlaveRuntime.cpp"
        "idf_slave/InterChipHostStub.cpp"
        "SpiProtocol.cpp"
        "Logger.cpp"
        "SettingsManager.cpp"
        "DeviceStore.cpp"
        "DeviceTopicMap.cpp"
        "UserStore.cpp"
        "FoundDeviceList.cpp"
        "InterChipSlave.cpp"
        "ZigbeeCoordinator.cpp"
        "FirmwareOta.cpp"
        "JoinedFirmwareZip.cpp"
        "StatusRgb.cpp"
    )
    set(ROLE_REQUIRES
        driver
        joltwallet__littlefs
        nvs_flash
        app_update
        spi_flash
        esp_timer
        arduino_zigbee
        espressif__esp-zboss-lib
        espressif__esp-zigbee-lib
    )
    idf_component_register(
        SRCS ${ROLE_SRCS}
        INCLUDE_DIRS "../include/idf_compat" "." "../include"
        REQUIRES ${ROLE_REQUIRES}
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BOARD_ROLE_SLAVE=1
        ZIGBEE_MODE_ZCZR=1
    )
else()
    set(HOST_IDF_SRCS
        "idf_host/app_main.cpp"
        "idf_host/HostSlaveReset.cpp"
        "idf_host/HostFs.cpp"
        "idf_host/HostAuth.cpp"
        "idf_host/HostHttpServer.cpp"
        "idf_host/HostServices.cpp"
        "idf_host/HostProductApi.cpp"
        "idf_host/HostSmtp.cpp"
        "idf_host/HostOnvifStub.cpp"
        "SpiProtocol.cpp"
        "Logger.cpp"
        "SettingsManager.cpp"
        "DeviceStore.cpp"
        "DeviceTopicMap.cpp"
        "UserStore.cpp"
        "ConsoleStore.cpp"
        "AutomationStore.cpp"
        "SceneStore.cpp"
        "CameraStore.cpp"
        "InterconnectStore.cpp"
        "SdCardStorage.cpp"
    )

    set(HOST_IDF_REQUIRES
        driver
        joltwallet__littlefs
        nvs_flash
        esp_netif
        esp_event
        esp_http_server
        mqtt
        app_update
        spi_flash
        esp_timer
        mbedtls
    )

    if(CONFIG_IDF_TARGET_ESP32S3)
        list(APPEND HOST_IDF_SRCS "idf_host/HostWifiS3.cpp")
        list(APPEND HOST_IDF_REQUIRES esp_wifi)
    elseif(CONFIG_IDF_TARGET_ESP32P4)
        list(APPEND HOST_IDF_SRCS "idf_host/HostWifiP4.cpp")
        list(APPEND HOST_IDF_REQUIRES espressif__esp_wifi_remote espressif__esp_hosted)
    endif()

    idf_component_register(
        SRCS ${HOST_IDF_SRCS}
        INCLUDE_DIRS "../include/idf_compat" "." "../include"
        REQUIRES ${HOST_IDF_REQUIRES}
    )
endif()
