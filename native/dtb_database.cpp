/*
 * Functional block database implementation.
 * Extracts, catalogs, and compares DTB blocks by function.
 */
#include "dtb_database.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>

// ============================================================================
// Utility Helpers
// ============================================================================

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](char c) { return (char)std::tolower((unsigned char)c); });
    return s;
}

static bool contains(const std::string& s, const std::string& q) {
    return lower(s).find(lower(q)) != std::string::npos;
}

static std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

// ============================================================================
// DtbDatabase Implementation
// ============================================================================

DtbDatabase::DtbDatabase() {}
DtbDatabase::~DtbDatabase() {}

bool DtbDatabase::load_from_json(const std::string& path) {
    // TODO: Implement JSON deserialization
    // For now, return false to indicate no persistent storage yet
    return false;
}

bool DtbDatabase::save_to_json(const std::string& path) {
    // TODO: Implement JSON serialization
    return false;
}

std::vector<std::shared_ptr<FunctionalBlock>> DtbDatabase::get_blocks_by_type(BlockType type) {
    std::vector<std::shared_ptr<FunctionalBlock>> result;
    for (const auto& kv : blocks_by_id) {
        if (kv.second && kv.second->type == type)
            result.push_back(kv.second);
    }
    return result;
}

std::shared_ptr<HardwareProfile> DtbDatabase::get_profile(const std::string& device_id) {
    auto it = profiles_by_device.find(device_id);
    if (it != profiles_by_device.end())
        return it->second;
    return nullptr;
}

std::vector<std::string> DtbDatabase::list_devices() {
    std::vector<std::string> result;
    for (const auto& kv : profiles_by_device)
        result.push_back(kv.first);
    return result;
}

int DtbDatabase::block_count() const {
    return (int)blocks_by_id.size();
}

int DtbDatabase::device_count() const {
    return (int)profiles_by_device.size();
}

std::string DtbDatabase::compute_block_id(const std::string& path,
                                          const std::map<std::string, std::string>& props) {
    // Simple ID: path hash + first property
    std::string id = path;
    if (!props.empty())
        id += "|" + props.begin()->first;
    return id;
}

int DtbDatabase::score_compatibility(const FunctionalBlock& donor,
                                     const FunctionalBlock& receiver) {
    if (donor.type != receiver.type)
        return 0;  // Different types: incompatible

    // Count matching critical properties
    int matches = 0;
    int total_critical = 0;

    for (const auto& dprop : donor.properties) {
        if (!dprop.critical)
            continue;
        ++total_critical;

        for (const auto& rprop : receiver.properties) {
            if (rprop.name == dprop.name && rprop.value == dprop.value) {
                ++matches;
                break;
            }
        }
    }

    if (total_critical == 0)
        return 50;  // No critical properties to match

    return (matches * 100) / total_critical;
}

// ============================================================================
// Extraction: Control Blocks
// ============================================================================

void DtbDatabase::extract_control_blocks(const DtbNode& root, HardwareProfile& profile) {
    // Look for nodes with GPIO/button/joystick names and GPIO properties
    std::vector<const DtbNode*> all_nodes;
    std::function<void(const DtbNode&)> collect = [&](const DtbNode& n) {
        all_nodes.push_back(&n);
        for (const auto& kv : n.children)
            collect(kv.second);
    };
    collect(root);

    std::set<std::string> seen;

    for (const DtbNode* n : all_nodes) {
        std::string fullpath = n->path + "/" + n->name;
        std::string lower_path = lower(fullpath);

        // Identify control nodes
        bool is_joystick = contains(lower_path, "joystick") || contains(lower_path, "joypad");
        bool is_button = contains(lower_path, "button") || contains(lower_path, "key");
        bool has_gpio = false;

        for (const auto& kv : n->properties) {
            if (kv.first == "gpios" || kv.first == "gpio" || contains(kv.first, "-gpios")) {
                has_gpio = true;
                break;
            }
        }

        if (!((is_joystick || is_button) && has_gpio))
            continue;

        std::string block_id = compute_block_id(n->path, {});
        if (!seen.insert(block_id).second)
            continue;

        auto block = std::make_shared<ControlBlock>();
        block->id = block_id;
        block->type = BlockType::CONTROL;
        block->path = n->path;
        block->name = n->name;
        block->label = n->label;
        block->description = "Control: " + n->name;

        // Extract GPIO pins
        for (const auto& kv : n->properties) {
            if (kv.first == "gpios" || kv.first == "gpio") {
                block->gpio_pins.push_back(kv.second.value);
                FunctionalProperty prop;
                prop.name = kv.first;
                prop.value = kv.second.value;
                prop.critical = true;
                block->properties.push_back(prop);
            }
        }

        // Determine control type
        if (is_joystick) {
            if (contains(lower_path, "adc"))
                block->control_type = ControlType::JOYSTICK_ADC;
            else
                block->control_type = ControlType::JOYSTICK_GPIO;
        } else {
            if (contains(lower_path, "adc"))
                block->control_type = ControlType::BUTTON_ADC;
            else
                block->control_type = ControlType::BUTTON_GPIO;
        }

        blocks_by_id[block_id] = block;
        profile.blocks.push_back(block);
        profile.has_gpio_buttons = true;
    }
}

// ============================================================================
// Extraction: Audio Blocks
// ============================================================================

void DtbDatabase::extract_audio_blocks(const DtbNode& root, HardwareProfile& profile) {
    std::vector<const DtbNode*> all_nodes;
    std::function<void(const DtbNode&)> collect = [&](const DtbNode& n) {
        all_nodes.push_back(&n);
        for (const auto& kv : n.children)
            collect(kv.second);
    };
    collect(root);

    std::set<std::string> seen;

    for (const DtbNode* n : all_nodes) {
        std::string lower_path = lower(n->path + "/" + n->name);

        bool is_audio = contains(lower_path, "audio") || contains(lower_path, "sound") ||
                        contains(lower_path, "codec") || contains(lower_path, "i2s") ||
                        contains(lower_path, "dai");

        if (!is_audio)
            continue;

        std::string block_id = compute_block_id(n->path, {});
        if (!seen.insert(block_id).second)
            continue;

        auto block = std::make_shared<AudioBlock>();
        block->id = block_id;
        block->type = BlockType::AUDIO;
        block->path = n->path;
        block->name = n->name;
        block->label = n->label;
        block->description = "Audio: " + n->name;

        // Extract properties
        for (const auto& kv : n->properties) {
            FunctionalProperty prop;
            prop.name = kv.first;
            prop.value = strip_quotes(kv.second.value);
            prop.critical = contains(kv.first, "compatible") || contains(kv.first, "codec");
            block->properties.push_back(prop);

            if (contains(lower_path, "rk817"))
                block->codec = AudioCodec::RK817;
        }

        blocks_by_id[block_id] = block;
        profile.blocks.push_back(block);
        profile.has_audio = true;
    }
}

// ============================================================================
// Extraction: Display Blocks
// ============================================================================

void DtbDatabase::extract_display_blocks(const DtbNode& root, HardwareProfile& profile) {
    std::vector<const DtbNode*> all_nodes;
    std::function<void(const DtbNode&)> collect = [&](const DtbNode& n) {
        all_nodes.push_back(&n);
        for (const auto& kv : n.children)
            collect(kv.second);
    };
    collect(root);

    std::set<std::string> seen;

    for (const DtbNode* n : all_nodes) {
        std::string lower_path = lower(n->path + "/" + n->name);

        bool is_display = contains(lower_path, "display") || contains(lower_path, "panel") ||
                          contains(lower_path, "backlight") || contains(lower_path, "dsi") ||
                          contains(lower_path, "mipi");

        if (!is_display)
            continue;

        std::string block_id = compute_block_id(n->path, {});
        if (!seen.insert(block_id).second)
            continue;

        auto block = std::make_shared<DisplayBlock>();
        block->id = block_id;
        block->type = BlockType::DISPLAY;
        block->path = n->path;
        block->name = n->name;
        block->label = n->label;
        block->description = "Display: " + n->name;

        // Extract properties
        for (const auto& kv : n->properties) {
            FunctionalProperty prop;
            prop.name = kv.first;
            prop.value = strip_quotes(kv.second.value);
            prop.critical = contains(kv.first, "compatible");
            block->properties.push_back(prop);
        }

        blocks_by_id[block_id] = block;
        profile.blocks.push_back(block);
        profile.has_display = true;
    }
}

// ============================================================================
// Extraction: Power Blocks
// ============================================================================

void DtbDatabase::extract_power_blocks(const DtbNode& root, HardwareProfile& profile) {
    std::vector<const DtbNode*> all_nodes;
    std::function<void(const DtbNode&)> collect = [&](const DtbNode& n) {
        all_nodes.push_back(&n);
        for (const auto& kv : n.children)
            collect(kv.second);
    };
    collect(root);

    std::set<std::string> seen;

    for (const DtbNode* n : all_nodes) {
        std::string lower_path = lower(n->path + "/" + n->name);

        bool is_power = contains(lower_path, "battery") || contains(lower_path, "charger") ||
                        contains(lower_path, "power") || contains(lower_path, "adc");

        if (!is_power)
            continue;

        std::string block_id = compute_block_id(n->path, {});
        if (!seen.insert(block_id).second)
            continue;

        auto block = std::make_shared<PowerBlock>();
        block->id = block_id;
        block->type = BlockType::POWER;
        block->path = n->path;
        block->name = n->name;
        block->label = n->label;
        block->description = "Power: " + n->name;

        // Extract properties
        for (const auto& kv : n->properties) {
            FunctionalProperty prop;
            prop.name = kv.first;
            prop.value = strip_quotes(kv.second.value);
            block->properties.push_back(prop);
        }

        blocks_by_id[block_id] = block;
        profile.blocks.push_back(block);
        profile.has_battery = true;
    }
}

// ============================================================================
// Catalog DTB
// ============================================================================

bool DtbDatabase::catalog_dtb(const DtbNode& root, const std::string& device_id,
                              const std::string& display_name) {
    auto profile = std::make_shared<HardwareProfile>();
    profile->device_id = device_id;
    profile->display_name = display_name;

    // Extract compatible field
    auto it = root.properties.find("compatible");
    if (it != root.properties.end())
        profile->compatible = strip_quotes(it->second.value);

    // Extract all block types
    extract_control_blocks(root, *profile);
    extract_audio_blocks(root, *profile);
    extract_display_blocks(root, *profile);
    extract_power_blocks(root, *profile);

    profiles_by_device[device_id] = profile;
    return true;
}

// ============================================================================
// Comparison Helper
// ============================================================================

BlockCompatibilityReport compare_blocks(const FunctionalBlock& donor,
                                        const FunctionalBlock& receiver) {
    BlockCompatibilityReport report;
    report.donor_id = donor.id;
    report.receiver_id = receiver.id;

    if (donor.type != receiver.type) {
        report.compatibility_score = 0;
        report.recommendation = "INCOMPATIBLE";
        report.transfer_warnings.push_back("Block types differ");
        return report;
    }

    // Count matching properties
    int matches = 0;
    int total = 0;

    for (const auto& dprop : donor.properties) {
        ++total;
        for (const auto& rprop : receiver.properties) {
            if (rprop.name == dprop.name && rprop.value == dprop.value) {
                matches++;
                report.matching_properties.push_back(dprop.name);
                break;
            } else if (rprop.name == dprop.name) {
                report.mismatched_properties.push_back(dprop.name + " (value differs)");
            }
        }
    }

    report.compatibility_score = total > 0 ? (matches * 100) / total : 50;

    if (report.compatibility_score >= 80)
        report.recommendation = "COMPATIBLE";
    else if (report.compatibility_score >= 50)
        report.recommendation = "PARTIAL";
    else
        report.recommendation = "INCOMPATIBLE";

    return report;
}
