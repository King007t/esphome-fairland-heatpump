#include "fairland_errors.h"

namespace esphome {
namespace fairland_climate {

namespace {

struct ErrorEntry {
  const char *code;
  const char *text;
};

// Index = discrete input - 48. Codes according to the Modbus protocol of the MWH216/MWH298
// control boards, descriptions according to the Fairland user manual ("Failure code" table).
// E3, E5, E6, Eb and Ed are protection messages, not device failures.
const ErrorEntry ERROR_TABLE[48] = {
    {"E0", "Unknown code E0"},                                                        // 48
    {"E1", "High pressure protection"},                                               // 49
    {"E2", "Low pressure protection"},                                                // 50
    {"E3", "No water flow (protection)"},                                             // 51
    {"E4", "3-phase sequence protection (three-phase units only)"},                   // 52
    {"E5", "Power supply outside operating range (protection)"},                      // 53
    {"E6", "Insufficient water flow, excessive inlet/outlet temperature difference"}, // 54
    {"E7", "Water outlet temperature too high or too low"},                           // 55
    {"E8", "High exhaust temperature protection"},                                    // 56
    {"E9", "Unknown code E9"},                                                        // 57
    {"EA", "Evaporator overheat protection (cooling mode)"},                          // 58
    {"Eb", "Ambient temperature too high or too low (protection)"},                   // 59
    {nullptr, nullptr},                                                               // 60 reserved
    {"Ed", "Anti-freezing reminder"},                                                 // 61
    {nullptr, nullptr},                                                               // 62 reserved
    {nullptr, nullptr},                                                               // 63 reserved
    {"P0", "Controller communication failure"},                                       // 64
    {"P1", "Water inlet temperature sensor failure"},                                 // 65
    {"P2", "Water outlet temperature sensor failure"},                                // 66
    {"P3", "Gas exhaust temperature sensor failure"},                                 // 67
    {"P4", "Evaporator coil temperature sensor failure"},                             // 68
    {"P5", "Gas return temperature sensor failure"},                                  // 69
    {"P6", "Cooling coil temperature sensor failure"},                                // 70
    {"P7", "Ambient temperature sensor failure"},                                     // 71
    {"P8", "Cooling plate sensor failure"},                                           // 72
    {"P9", "Current sensor failure"},                                                 // 73
    {"PA", "Restart memory failure"},                                                 // 74
    {nullptr, nullptr},                                                               // 75 reserved
    {nullptr, nullptr},                                                               // 76 reserved
    {nullptr, nullptr},                                                               // 77 reserved
    {nullptr, nullptr},                                                               // 78 reserved
    {nullptr, nullptr},                                                               // 79 reserved
    {"F0", "Unknown code F0"},                                                        // 80
    {"F1", "Compressor drive module failure"},                                        // 81
    {"F2", "PFC module failure"},                                                     // 82
    {"F3", "Compressor start failure"},                                               // 83
    {"F4", "Compressor running failure"},                                             // 84
    {"F5", "Inverter board over-current protection"},                                 // 85
    {"F6", "Inverter board overheat protection"},                                     // 86
    {"F7", "Current protection"},                                                     // 87
    {"F8", "Cooling plate overheat protection"},                                      // 88
    {"F9", "Fan motor failure"},                                                      // 89
    {"FA", "PFC module over-current protection"},                                     // 90
    {"Fb", "Power filter board no-power protection"},                                 // 91
    {nullptr, nullptr},                                                               // 92 reserved
    {nullptr, nullptr},                                                               // 93 reserved
    {nullptr, nullptr},                                                               // 94 reserved
    {nullptr, nullptr},                                                               // 95 reserved
};

const uint8_t FIRST_ADDRESS = 48;
const uint8_t E3_INDEX = 51 - FIRST_ADDRESS;

}  // namespace

const char *fairland_error_code(uint8_t address) {
  if (address < FIRST_ADDRESS || address >= FIRST_ADDRESS + 48)
    return nullptr;
  return ERROR_TABLE[address - FIRST_ADDRESS].code;
}

const char *fairland_error_text(uint8_t address) {
  if (address < FIRST_ADDRESS || address >= FIRST_ADDRESS + 48)
    return nullptr;
  return ERROR_TABLE[address - FIRST_ADDRESS].text;
}

std::string decode_fairland_errors(std::span<const uint8_t> data, bool &any_active, bool &e3_active) {
  any_active = false;
  e3_active = false;
  std::string result;

  const size_t bit_count = data.size() * 8;
  for (size_t bit = 0; bit < 48 && bit < bit_count; bit++) {
    if ((data[bit / 8] & (1u << (bit % 8))) == 0)
      continue;
    any_active = true;
    if (bit == E3_INDEX)
      e3_active = true;

    if (!result.empty())
      result += " | ";
    const ErrorEntry &entry = ERROR_TABLE[bit];
    if (entry.code != nullptr) {
      result += entry.code;
      result += ": ";
      result += entry.text;
    } else {
      result += "Unknown (DI ";
      result += std::to_string(FIRST_ADDRESS + bit);
      result += ")";
    }
  }

  if (result.empty())
    result = "No fault";
  return result;
}

}  // namespace fairland_climate
}  // namespace esphome
