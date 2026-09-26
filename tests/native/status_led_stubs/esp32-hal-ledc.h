#pragma once
#include <cstdint>
enum ledc_clk_cfg_t { LEDC_AUTO_CLK, LEDC_USE_XTAL_CLK };
ledc_clk_cfg_t ledcGetClockSource();
bool ledcSetClockSource(ledc_clk_cfg_t);
bool ledcAttach(uint8_t, uint32_t, uint8_t);
bool ledcWrite(uint8_t, uint32_t);
bool ledcDetach(uint8_t);
uint32_t ledcRead(uint8_t);
uint32_t ledcReadFreq(uint8_t);
