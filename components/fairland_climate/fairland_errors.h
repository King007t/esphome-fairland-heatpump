#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace esphome {
namespace fairland_climate {

/**
 * Decodes the Fairland error block: discrete inputs 48-95 read as one 48-bit block
 * (Modbus FC02, start address 48, count 48). `data` holds the packed bits of the
 * response (bit 0 of byte 0 = DI 48).
 *
 * @param data        response payload (6 bytes)
 * @param any_active  set to true when at least one code is active
 * @param e3_active   set to true when E3 (no water flow) is active
 * @return plain text, several codes separated by " | ", otherwise "No fault"
 */
std::string decode_fairland_errors(std::span<const uint8_t> data, bool &any_active, bool &e3_active);

/// Code (e.g. "E3") or plain text for discrete input `address` (48-95); nullptr for reserved bits.
const char *fairland_error_code(uint8_t address);
const char *fairland_error_text(uint8_t address);

}  // namespace fairland_climate
}  // namespace esphome
