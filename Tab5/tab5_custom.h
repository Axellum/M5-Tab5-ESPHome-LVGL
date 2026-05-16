#pragma once
#include "esphome.h"
#include <string>

extern std::string cal_jour_nom[15];
extern std::string cal_heures[15];
extern bool cal_toggled[15];

void tab5_calendar_toggle(int jour);
namespace esphome { namespace font { class Font; } }
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);

uint32_t get_humidity_color(float x);
uint32_t get_temperature_color(float t);
