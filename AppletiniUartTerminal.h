#pragma once

#include "nlohmann/json.hpp"

void appletini_uart_terminal_imgui_window(bool* p_open);
nlohmann::json appletini_uart_terminal_serialize();
void appletini_uart_terminal_deserialize(const nlohmann::json& jsonState);
