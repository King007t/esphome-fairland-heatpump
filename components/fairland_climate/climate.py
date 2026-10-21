import esphome.codegen as cg
from esphome.components import climate, number, select, sensor, switch
import esphome.config_validation as cv

fairland_climate_ns = cg.esphome_ns.namespace("fairland_climate")
FairlandClimate = fairland_climate_ns.class_(
    "FairlandClimate", climate.Climate, cg.Component
)

CONF_POWER_SWITCH = "power_switch"
CONF_MODE_SELECT = "mode_select"
CONF_WORKING_MODE_SELECT = "working_mode_select"
CONF_HEATING_TARGET_NUMBER = "heating_target_number"
CONF_COOLING_TARGET_NUMBER = "cooling_target_number"
CONF_AUTO_TARGET_NUMBER = "auto_target_number"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_COMPRESSOR_SENSOR = "compressor_sensor"
CONF_POWER_ON_DELAY = "power_on_delay"
CONF_MODE_OPTIONS = "mode_options"
CONF_AUTO = "auto"
CONF_HEAT = "heat"
CONF_COOL = "cool"

MODE_OPTIONS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_AUTO, default="Auto"): cv.string_strict,
        cv.Optional(CONF_HEAT, default="Heating"): cv.string_strict,
        cv.Optional(CONF_COOL, default="Cooling"): cv.string_strict,
    }
)

CONFIG_SCHEMA = (
    climate.climate_schema(FairlandClimate)
    .extend(
        {
            cv.Required(CONF_POWER_SWITCH): cv.use_id(switch.Switch),
            cv.Required(CONF_MODE_SELECT): cv.use_id(select.Select),
            cv.Required(CONF_WORKING_MODE_SELECT): cv.use_id(select.Select),
            cv.Required(CONF_HEATING_TARGET_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_COOLING_TARGET_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_AUTO_TARGET_NUMBER): cv.use_id(number.Number),
            cv.Required(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_COMPRESSOR_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(
                CONF_POWER_ON_DELAY, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MODE_OPTIONS, default={}): MODE_OPTIONS_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    cg.add(var.set_power_switch(await cg.get_variable(config[CONF_POWER_SWITCH])))
    cg.add(var.set_mode_select(await cg.get_variable(config[CONF_MODE_SELECT])))
    cg.add(
        var.set_working_mode_select(
            await cg.get_variable(config[CONF_WORKING_MODE_SELECT])
        )
    )
    cg.add(
        var.set_heating_target_number(
            await cg.get_variable(config[CONF_HEATING_TARGET_NUMBER])
        )
    )
    if CONF_COOLING_TARGET_NUMBER in config:
        cg.add(
            var.set_cooling_target_number(
                await cg.get_variable(config[CONF_COOLING_TARGET_NUMBER])
            )
        )
    if CONF_AUTO_TARGET_NUMBER in config:
        cg.add(
            var.set_auto_target_number(
                await cg.get_variable(config[CONF_AUTO_TARGET_NUMBER])
            )
        )
    cg.add(
        var.set_current_temperature_sensor(
            await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        )
    )
    if CONF_COMPRESSOR_SENSOR in config:
        cg.add(
            var.set_compressor_sensor(
                await cg.get_variable(config[CONF_COMPRESSOR_SENSOR])
            )
        )
    cg.add(var.set_power_on_delay(config[CONF_POWER_ON_DELAY]))
    mode_options = config[CONF_MODE_OPTIONS]
    cg.add(
        var.set_mode_options(
            mode_options[CONF_AUTO], mode_options[CONF_HEAT], mode_options[CONF_COOL]
        )
    )
