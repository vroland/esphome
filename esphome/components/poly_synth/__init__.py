from esphome import automation
import esphome.codegen as cg
from esphome.components import matrix_keypad, speaker
from esphome.components.const import CONF_KEYS
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OUTPUT_SPEAKER, CONF_SAMPLE_RATE, PLATFORM_ESP32
from esphome.core import ID
from esphome.types import ConfigType

DEPENDENCIES = ["speaker"]
AUTO_LOAD = ["audio"]
CODEOWNERS = ["@vroland"]

ns = cg.esphome_ns.namespace("poly_synth")
PolySynth = ns.class_("PolySynth", cg.Component)
MatrixKeypadAdapter = ns.class_("MatrixKeypadAdapter")
NoteOnAction = ns.class_("NoteOnAction", automation.Action)
NoteOffAction = ns.class_("NoteOffAction", automation.Action)
AllNotesOffAction = ns.class_("AllNotesOffAction", automation.Action)

CONF_POLYPHONY = "polyphony"
CONF_GAIN = "gain"
CONF_OSCILLATOR = "oscillator"
CONF_WAVEFORM = "waveform"
CONF_DETUNE_CENTS = "detune_cents"
CONF_AMP_ENVELOPE = "amp_envelope"
CONF_FILTER = "filter"
CONF_CUTOFF = "cutoff"
CONF_RESONANCE = "resonance"
CONF_FILTER_ENVELOPE = "filter_envelope"
CONF_AMOUNT = "amount"
CONF_ATTACK = "attack"
CONF_DECAY = "decay"
CONF_SUSTAIN = "sustain"
CONF_RELEASE = "release"
CONF_KEYPAD = "keypad"
CONF_BASE_NOTE = "base_note"
CONF_VELOCITY = "velocity"


def envelope(default_attack, default_decay, default_sustain, default_release):
    return cv.Schema(
        {
            cv.Optional(CONF_ATTACK, default=default_attack): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DECAY, default=default_decay): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SUSTAIN, default=default_sustain): cv.percentage,
            cv.Optional(CONF_RELEASE, default=default_release): cv.positive_time_period_milliseconds,
        }
    )


def validate_keypad_mapping(config: ConfigType) -> ConfigType:
    keys = config[CONF_KEYS]
    if not keys:
        raise cv.Invalid("keypad keys must not be empty")
    if any(ord(key) > 127 or ord(key) == 0 for key in keys):
        raise cv.Invalid("keypad keys must be single-byte ASCII characters")
    if len(set(keys)) != len(keys):
        raise cv.Invalid("keypad keys must be unique")
    if config[CONF_BASE_NOTE] + len(keys) - 1 > 127:
        raise cv.Invalid("keypad keys exceed the MIDI note range (0-127)")
    return config


KEYPAD_MAPPING_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_KEYS): cv.string,
        cv.Optional(CONF_BASE_NOTE, default=69): cv.int_range(min=0, max=127),
        cv.Optional(CONF_VELOCITY, default=1.0): cv.float_range(min=0.0, max=1.0),
    }
)

KEYPAD_SCHEMA = cv.All(
    KEYPAD_MAPPING_SCHEMA.extend(
        {cv.Required(CONF_ID): cv.use_id(matrix_keypad.MatrixKeypad)}
    ),
    validate_keypad_mapping,
)


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PolySynth),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.one_of(48000, int=True),
            cv.Optional(CONF_POLYPHONY, default=12): cv.int_range(min=1, max=16),
            cv.Optional(CONF_GAIN, default=0.30): cv.float_range(min=0.0, max=1.0),
            cv.Optional(CONF_OSCILLATOR, default={}): cv.Schema(
                {
                    cv.Optional(CONF_WAVEFORM, default="polyblep_saw"): cv.one_of("polyblep_saw"),
                    cv.Optional(CONF_DETUNE_CENTS, default=7): cv.float_range(min=-100, max=100),
                }
            ),
            cv.Optional(CONF_AMP_ENVELOPE, default={}): envelope("5ms", "250ms", "55%", "300ms"),
            cv.Optional(CONF_FILTER, default={}): cv.Schema(
                {
                    cv.Optional("type", default="low_pass"): cv.one_of("low_pass"),
                    cv.Optional(CONF_CUTOFF, default="1500Hz"): cv.All(cv.frequency, cv.float_range(min=20, max=15000)),
                    cv.Optional(CONF_RESONANCE, default=0.15): cv.float_range(min=0, max=1),
                }
            ),
            cv.Optional(CONF_FILTER_ENVELOPE, default={}): envelope("2ms", "300ms", "0%", "200ms").extend(
                {cv.Optional(CONF_AMOUNT, default="2500Hz"): cv.All(cv.frequency, cv.float_range(min=0, max=12000))}
            ),
            cv.Optional(CONF_KEYPAD): KEYPAD_SCHEMA,
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_output_speaker(await cg.get_variable(config[CONF_OUTPUT_SPEAKER])))
    cg.add(var.set_polyphony(config[CONF_POLYPHONY]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_detune(config[CONF_OSCILLATOR][CONF_DETUNE_CENTS]))
    a = config[CONF_AMP_ENVELOPE]
    cg.add(var.set_amp_envelope(a[CONF_ATTACK].total_milliseconds / 1000, a[CONF_DECAY].total_milliseconds / 1000, a[CONF_SUSTAIN], a[CONF_RELEASE].total_milliseconds / 1000))
    f = config[CONF_FILTER]
    cg.add(var.set_filter(f[CONF_CUTOFF], f[CONF_RESONANCE]))
    e = config[CONF_FILTER_ENVELOPE]
    cg.add(var.set_filter_envelope(e[CONF_AMOUNT], e[CONF_ATTACK].total_milliseconds / 1000, e[CONF_DECAY].total_milliseconds / 1000, e[CONF_SUSTAIN], e[CONF_RELEASE].total_milliseconds / 1000))
    if (keypad_config := config.get(CONF_KEYPAD)) is not None:
        cg.add_define("USE_POLY_SYNTH_KEYPAD")
        keypad = await cg.get_variable(keypad_config[CONF_ID])
        bridge = cg.new_Pvariable(
            ID(f"{config[CONF_ID]}_keypad_adapter", type=MatrixKeypadAdapter),
            var,
            keypad_config[CONF_KEYS],
            keypad_config[CONF_BASE_NOTE],
            keypad_config[CONF_VELOCITY],
        )
        cg.add(keypad.register_listener(bridge))


NOTE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(PolySynth),
        cv.Required("note"): cv.templatable(cv.int_range(min=0, max=127)),
    }
)


@automation.register_action(
    "poly_synth.note_on",
    NoteOnAction,
    NOTE_SCHEMA.extend({cv.Optional("velocity", default=1.0): cv.templatable(cv.float_range(min=0, max=1))}),
    synchronous=True,
)
async def note_on_action_to_code(config, action_id, template_arg, args):
    synth = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, synth)
    note = await cg.templatable(config["note"], args, cg.uint8)
    velocity = await cg.templatable(config["velocity"], args, cg.float_)
    cg.add(var.set_note(note))
    cg.add(var.set_velocity(velocity))
    return var


@automation.register_action(
    "poly_synth.note_off", NoteOffAction, NOTE_SCHEMA, synchronous=True
)
async def note_off_action_to_code(config, action_id, template_arg, args):
    synth = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, synth)
    note = await cg.templatable(config["note"], args, cg.uint8)
    cg.add(var.set_note(note))
    return var


@automation.register_action(
    "poly_synth.all_notes_off",
    AllNotesOffAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(PolySynth)}),
    synchronous=True,
)
async def all_notes_off_action_to_code(config, action_id, template_arg, args):
    synth = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, synth)
