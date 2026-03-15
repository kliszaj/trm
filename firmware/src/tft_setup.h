#pragma once
// TFT_eSPI configuration for Seeed XIAO Round Display (GC9A01 + XIAO ESP32-C3)
// Pin mapping for Seeed Studio Round Display for XIAO

#define GC9A01_DRIVER

#define TFT_RGB_ORDER TFT_RGB

#define TFT_MOSI 10
#define TFT_MISO 9
#define TFT_SCLK 8
#define TFT_CS   3   // D1 = GPIO3
#define TFT_DC   5   // D3 = GPIO5
#define TFT_BL   21  // D6 = GPIO21
#define TFT_RST  2   // D0 = GPIO2

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  20000000
