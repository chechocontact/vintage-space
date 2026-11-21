#include "Presets.h"

namespace voxlift
{
namespace
{
constexpr Range off {};
constexpr Range openTop { 20000.0f, 20000.0f, {}, true };
constexpr Range unity { 1.0f, 1.0f };

// Voice-specific choices shared by every preset:
// - The high-pass stops between ~600 Hz and ~2.2 kHz: higher than that leaves only sibilance.
// - Resonance never passes Q 1.6, so the sweep does not whistle on sustained vowels.
// - Echo and reverb duck under the dry voice so the words stay intelligible early in the build.
// - Noise follows the singer's envelope (breath), so a silent track stays silent.
constexpr std::array<Preset, numPresets> presets {{
    {
        .name = "Vocal Lift",
        .description = "Soft pop build: rising high-pass, short echoes, light room.",
        .hpHz = { 20.0f, 900.0f, { 0.0f, 1.0f, 1.4f }, true },
        .hpQ = { 0.707f, 1.0f },
        .lpHz = openTop,
        .saturation = { 0.0f, 0.25f, { 0.3f, 1.0f, 1.0f } },
        .gateDepth = off,
        .dryGain = { 1.0f, 0.85f },
        .delayBeats = 0.75f,
        .delaySend = { 0.0f, 0.25f, { 0.05f, 0.8f, 1.0f } },
        .delayFeedback = { 0.15f, 0.45f },
        .shiftHz = off,
        .reverbSend = { 0.0f, 0.45f, { 0.0f, 0.9f, 1.2f } },
        .reverbSize = { 0.55f, 0.85f },
        .reverbDamping = 0.6f,
        .noise = { 0.0f, 0.03f, { 0.4f, 1.0f, 1.5f } },
        .width = { 1.0f, 1.3f },
        .duck = { 0.6f, 0.3f },
    },
    {
        .name = "Airy Choir",
        .description = "Big wash for ballads and hooks: long reverb, slow rising echoes.",
        .hpHz = { 20.0f, 1400.0f, { 0.0f, 1.0f, 1.6f }, true },
        .hpQ = { 0.707f, 1.2f },
        .lpHz = openTop,
        .saturation = { 0.0f, 0.1f },
        .gateDepth = off,
        .dryGain = { 1.0f, 0.7f, { 0.2f, 1.0f, 1.0f } },
        .delayBeats = 1.0f,
        .delaySend = { 0.0f, 0.35f, { 0.1f, 0.9f, 1.0f } },
        .delayFeedback = { 0.3f, 0.6f },
        .shiftHz = { 0.0f, 8.0f, { 0.4f, 1.0f, 1.3f } },
        .reverbSend = { 0.0f, 0.7f, { 0.0f, 0.85f, 0.8f } },
        .reverbSize = { 0.75f, 0.95f },
        .reverbDamping = 0.7f,
        .noise = { 0.0f, 0.05f, { 0.3f, 1.0f, 1.5f } },
        .width = { 1.0f, 1.6f },
        .duck = { 0.5f, 0.1f },
    },
    {
        .name = "Radio Rise",
        .description = "Narrowing telephone band with grit and an 8th-to-16th stutter.",
        .hpHz = { 20.0f, 1200.0f, { 0.0f, 1.0f, 1.2f }, true },
        .hpQ = { 0.707f, 1.6f },
        .lpHz = { 20000.0f, 3500.0f, { 0.0f, 0.9f, 0.8f }, true },
        .saturation = { 0.0f, 0.6f, { 0.1f, 1.0f, 1.0f } },
        .gateDepth = { 0.0f, 0.5f, { 0.5f, 1.0f, 1.0f } },
        .gateDivStartBeats = 0.5f,
        .gateDivEndBeats = 0.25f,
        .dryGain = { 1.0f, 0.9f },
        .delayBeats = 0.5f,
        .delaySend = { 0.0f, 0.2f, { 0.2f, 1.0f, 1.0f } },
        .delayFeedback = { 0.2f, 0.5f },
        .shiftHz = off,
        .reverbSend = { 0.0f, 0.3f, { 0.2f, 1.0f, 1.0f } },
        .reverbSize = { 0.5f, 0.75f },
        .reverbDamping = 0.5f,
        .noise = { 0.0f, 0.063f, { 0.2f, 1.0f, 1.2f } },
        .width = { 1.0f, 1.1f },
        .duck = { 0.4f, 0.2f },
    },
    {
        .name = "Stutter Build",
        .description = "Tempo-synced chop that speeds up from 1/8 to 1/32 while the voice rises.",
        .hpHz = { 20.0f, 1600.0f, { 0.0f, 1.0f, 1.3f }, true },
        .hpQ = { 0.707f, 1.3f },
        .lpHz = openTop,
        .saturation = { 0.0f, 0.3f, { 0.3f, 1.0f, 1.0f } },
        .gateDepth = { 0.0f, 0.9f, { 0.35f, 0.95f, 1.0f } },
        .gateDivStartBeats = 0.5f,
        .gateDivEndBeats = 0.125f,
        .dryGain = unity,
        .delayBeats = 0.25f,
        .delaySend = { 0.0f, 0.2f, { 0.2f, 1.0f, 1.0f } },
        .delayFeedback = { 0.2f, 0.4f },
        .shiftHz = off,
        .reverbSend = { 0.0f, 0.4f, { 0.1f, 1.0f, 1.0f } },
        .reverbSize = { 0.5f, 0.8f },
        .reverbDamping = 0.6f,
        .noise = { 0.0f, 0.04f, { 0.4f, 1.0f, 1.5f } },
        .width = { 1.0f, 1.4f },
        .duck = { 0.3f, 0.1f },
    },
    {
        .name = "Endless Spiral",
        .description = "Extreme: echoes that keep climbing in pitch, big hall, breath noise.",
        .hpHz = { 20.0f, 2200.0f, { 0.0f, 1.0f, 1.2f }, true },
        .hpQ = { 0.707f, 1.5f },
        .lpHz = openTop,
        .saturation = { 0.0f, 0.45f, { 0.2f, 1.0f, 1.0f } },
        .gateDepth = { 0.0f, 0.4f, { 0.6f, 1.0f, 1.0f } },
        .gateDivStartBeats = 0.25f,
        .gateDivEndBeats = 0.25f,
        .dryGain = { 1.0f, 0.75f },
        .delayBeats = 0.75f,
        .delaySend = { 0.0f, 0.5f, { 0.0f, 0.8f, 0.9f } },
        .delayFeedback = { 0.35f, 0.82f },
        .shiftHz = { 0.0f, 25.0f, { 0.15f, 1.0f, 1.2f } },
        .reverbSend = { 0.0f, 0.6f, { 0.0f, 0.9f, 1.0f } },
        .reverbSize = { 0.7f, 0.93f },
        .reverbDamping = 0.65f,
        .noise = { 0.0f, 0.1f, { 0.2f, 1.0f, 1.3f } },
        .width = { 1.0f, 1.7f },
        .duck = { 0.2f, 0.0f },
    },
    {
        .name = "Whisper Wash",
        .description = "Intimate: breathy noise, dark long reverb, the voice dissolves upward.",
        .hpHz = { 20.0f, 600.0f, { 0.0f, 1.0f, 1.5f }, true },
        .hpQ = { 0.707f, 0.9f },
        .lpHz = { 20000.0f, 9000.0f, { 0.3f, 1.0f, 1.0f }, true },
        .saturation = off,
        .gateDepth = off,
        .dryGain = { 1.0f, 0.6f, { 0.3f, 1.0f, 1.0f } },
        .delayBeats = 1.0f,
        .delaySend = { 0.0f, 0.25f, { 0.2f, 1.0f, 1.0f } },
        .delayFeedback = { 0.3f, 0.5f },
        .shiftHz = off,
        .reverbSend = { 0.0f, 0.75f, { 0.0f, 0.8f, 0.7f } },
        .reverbSize = { 0.8f, 0.96f },
        .reverbDamping = 0.85f,
        .noise = { 0.0f, 0.08f, { 0.1f, 1.0f, 1.0f } },
        .width = { 1.0f, 1.5f },
        .duck = { 0.5f, 0.2f },
    },
}};
} // namespace

const Preset& getPreset (int index) noexcept
{
    const auto clamped = index < 0 ? 0 : (index >= numPresets ? numPresets - 1 : index);
    return presets[(size_t) clamped];
}
} // namespace voxlift
