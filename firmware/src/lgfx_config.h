#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;

public:
    LGFX() {
        auto cfg = _bus.config();
        cfg.spi_host   = SPI2_HOST;
        cfg.spi_mode   = 0;
        cfg.freq_write = 40000000;
        cfg.pin_sclk   = 8;   // D8
        cfg.pin_mosi   = 10;  // D10
        cfg.pin_miso   = -1;
        cfg.pin_dc     = 5;   // D3
        _bus.config(cfg);
        _panel.setBus(&_bus);

        auto pcfg = _panel.config();
        pcfg.pin_cs   = 3;    // D1
        pcfg.pin_rst  = 2;    // D0
        pcfg.pin_busy = -1;
        pcfg.memory_width  = 240;
        pcfg.memory_height = 240;
        pcfg.panel_width   = 240;
        pcfg.panel_height  = 240;
        pcfg.offset_x      = 0;
        pcfg.offset_y      = 0;
        pcfg.offset_rotation = 0;
        pcfg.rgb_order     = false;
        pcfg.invert        = false;
        _panel.config(pcfg);

        auto lcfg = _light.config();
        lcfg.pin_bl   = 21;   // D6
        lcfg.invert   = false;
        lcfg.freq     = 44100;
        lcfg.pwm_channel = 7;
        _light.config(lcfg);
        _panel.setLight(&_light);

        setPanel(&_panel);
    }
};
