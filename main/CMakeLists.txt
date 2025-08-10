idf_component_register(
  SRCS
    "main.cpp"
    "ble/RaceBoxClient.cpp"
    "io/EncoderInput.cpp"
    "ui/Display.cpp"
    "ui/Menu.cpp"
  INCLUDE_DIRS
    "."
    "common"
    "ble"
    "io"
    "ui"
    "config"
  REQUIRES
    driver
    esp_timer
    nvs_flash
    bt
    spi_flash
)


