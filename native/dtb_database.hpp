/*
 * Functional block database and hardware profiles.
 *
 * Catalogs DTB blocks by function (controls, audio, display, power) 
 * independent of their location in the tree. Enables intelligent 
 * cross-device transfer planning and compatibility scoring.
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "dtb_model.hpp"

// ============================================================================
// Functional Block Types
// ============================================================================

enum class BlockType {
    CONTROL,      // GPIO keys, joysticks, buttons
    AUDIO,        // Codecs, I2S, routing
    DISPLAY,      // Panels, backlight, DSI
    POWER,        // Battery, charger, ADC
    OTHER
};

enum class ControlType {
    JOYSTICK_ADC,
    JOYSTICK_GPIO,
    BUTTON_GPIO,
    BUTTON_ADC,
    ROCKER,
    UNKNOWN_CONTROL
};

enum class AudioCodec {
    RK817,
    RT5645,
    ES8323,
    UNKNOWN_CODEC
};

// ============================================================================
// Core Structures
// ============================================================================

struct FunctionalProperty {
    std::string name;
    std::string value;
    std::string description;
    bool critical = false;  // Must match for compatibility
};

struct FunctionalBlock {
    std::string id;           // Unique identifier (hash of path + properties)
    BlockType type;
    std::string path;         // Original path in DTB
    std::string name;         // Node name
    std::string label;        // DTB label if present
    std::string description;  // Human-readable description
    
    std::vector<FunctionalProperty> properties;
    std::map<std::string, std::string> metadata;  // Extra fields
    
    // Compatibility hints
    int compatibility_score = 0;  // 0-100
    std::vector<std::string> compatible_with;  // Other block IDs
    std::string transfer_notes;
};

struct ControlBlock : FunctionalBlock {
    ControlType control_type;
    std::vector<std::string> gpio_pins;
    std::vector<int> linux_codes;
};

struct AudioBlock : FunctionalBlock {
    AudioCodec codec;
    std::vector<std::string> dai_names;  // i2s0, i2s1, etc
    std::vector<std::string> routing;    // Headphone, Speaker, etc
};

struct DisplayBlock : FunctionalBlock {
    std::string panel_type;       // ST7703, simple-panel, etc
    int width = 0;
    int height = 0;
    int lanes = 0;                // DSI lanes
    std::string backlight_type;
};

struct PowerBlock : FunctionalBlock {
    std::vector<std::string> adc_channels;
    std::vector<std::string> gpio_controls;
};

// ============================================================================
// Hardware Profile
// ============================================================================

struct HardwareProfile {
    std::string device_id;        // gkd-pixel2, gamemt-e6, rf3536k4ka, etc
    std::string display_name;
    std::string compatible;       // compatible field from DTB
    std::string chip;             // RK3326, RK3399, etc
    
    std::vector<std::shared_ptr<FunctionalBlock>> blocks;
    
    // Feature matrix
    bool has_analog_joystick = false;
    bool has_gpio_buttons = false;
    bool has_audio = false;
    bool has_display = false;
    bool has_battery = false;
};

// ============================================================================
// Database Manager
// ============================================================================

class DtbDatabase {
public:
    DtbDatabase();
    ~DtbDatabase();
    
    // Load/Save
    bool load_from_json(const std::string& path);
    bool save_to_json(const std::string& path);
    
    // Catalog from DTB
    bool catalog_dtb(const DtbNode& root, const std::string& device_id, 
                     const std::string& display_name);
    
    // Query
    std::vector<std::shared_ptr<FunctionalBlock>> get_blocks_by_type(BlockType type);
    std::shared_ptr<HardwareProfile> get_profile(const std::string& device_id);
    std::vector<std::string> list_devices();
    
    // Compatibility scoring
    int score_compatibility(const FunctionalBlock& donor, 
                           const FunctionalBlock& receiver);
    
    // Statistics
    int block_count() const;
    int device_count() const;
    
private:
    std::map<std::string, std::shared_ptr<FunctionalBlock>> blocks_by_id;
    std::map<std::string, std::shared_ptr<HardwareProfile>> profiles_by_device;
    
    // Internal extraction
    void extract_control_blocks(const DtbNode& root, HardwareProfile& profile);
    void extract_audio_blocks(const DtbNode& root, HardwareProfile& profile);
    void extract_display_blocks(const DtbNode& root, HardwareProfile& profile);
    void extract_power_blocks(const DtbNode& root, HardwareProfile& profile);
    
    std::string compute_block_id(const std::string& path, 
                                 const std::map<std::string, std::string>& props);
};

// ============================================================================
// Comparison Results
// ============================================================================

struct BlockCompatibilityReport {
    std::string donor_id;
    std::string receiver_id;
    int compatibility_score;      // 0-100
    std::string recommendation;   // "COMPATIBLE", "PARTIAL", "INCOMPATIBLE"
    std::vector<std::string> matching_properties;
    std::vector<std::string> mismatched_properties;
    std::vector<std::string> transfer_warnings;
};

// Helper function to compare two blocks
BlockCompatibilityReport compare_blocks(const FunctionalBlock& donor,
                                       const FunctionalBlock& receiver);
