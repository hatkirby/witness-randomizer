#pragma once

class Randomizer {
public:
    Randomizer(const std::shared_ptr<Memory>& memory);
    void Randomize();

private:
    int _lastRandomizedFrame = 1 << 30;

    void Tutorialise(int panel1, int copyFrom);

    std::shared_ptr<Memory> _memory;

    friend class SwapTests_Shipwreck_Test;
};

#define PATH_COLOR 0xC0 
#define REFLECTION_PATH_COLOR 0xD0 
#define DOT_COLOR 0xF0 
#define ACTIVE_COLOR 0x100 
#define BACKGROUND_REGION_COLOR 0x110 
#define SUCCESS_COLOR_A 0x120 
#define SUCCESS_COLOR_B 0x130 
#define STROBE_COLOR_A 0x140 
#define STROBE_COLOR_B 0x150 
#define ERROR_COLOR 0x160 
#define PATTERN_POINT_COLOR 0x180 
#define PATTERN_POINT_COLOR_A 0x190 
#define PATTERN_POINT_COLOR_B 0x1A0 
#define SYMBOL_A 0x1B0 
#define SYMBOL_B 0x1C0 
#define SYMBOL_C 0x1D0 
#define SYMBOL_D 0x1E0 
#define SYMBOL_E 0x1F0 
#define PUSH_SYMBOL_COLORS 0x200 
#define OUTER_BACKGROUND 0x204 
#define OUTER_BACKGROUND_MODE 0x214 
#define TRACED_EDGES 0x228 
#define TRACED_EDGE_DATA 0x230 
#define AUDIO_PREFIX 0x270 
#define POWER 0x2A0 
#define TARGET 0x2B4 
#define POWER_OFF_ON_FAIL 0x2B8
#define IS_CYLINDER 0x2F4
#define CYLINDER_Z0 0x2F8
#define CYLINDER_Z1 0x2FC
#define CYLINDER_RADIUS 0x300
#define CURSOR_SPEED_SCALE 0x350 
#define NEEDS_REDRAW 0x37C
#define SPECULAR_ADD 0x38C
#define SPECULAR_POWER 0x390
#define PATH_WIDTH_SCALE 0x39C 
#define STARTPOINT_SCALE 0x3A0 
#define NUM_DOTS 0x3B4 
#define NUM_CONNECTIONS 0x3B8 
#define MAX_BROADCAST_DISTANCE 0x3BC
#define DOT_POSITIONS 0x3C0 
#define DOT_FLAGS 0x3C8 
#define DOT_CONNECTION_A 0x3D0 
#define DOT_CONNECTION_B 0x3D8 
#define RANDOMISE_ON_POWER_ON 0x3E0
#define DECORATIONS 0x418 
#define DECORATION_FLAGS 0x420 
#define DECORATION_COLORS 0x428 
#define NUM_DECORATIONS 0x430 
#define REFLECTION_DATA 0x438 
#define GRID_SIZE_X 0x440 
#define GRID_SIZE_Y 0x444 
#define STYLE_FLAGS 0x448 
#define SEQUENCE_LEN 0x454 
#define SEQUENCE 0x458 
#define DOT_SEQUENCE_LEN 0x460 
#define DOT_SEQUENCE 0x468 
#define DOT_SEQUENCE_LEN_REFLECTION 0x470 
#define DOT_SEQUENCE_REFLECTION 0x478 
#define NUM_COLORED_REGIONS 0x498 
#define COLORED_REGIONS 0x4A0 
#define PANEL_TARGET 0x4A8 
#define SPECULAR_TEXTURE 0x4D0 
#define CABLE_TARGET_2 0xD0
#define AUDIO_LOG_NAME 0x0
#define OPEN_RATE 0xE0
#define METADATA 0x13A // sizeof(short)
#define HOTEL_EP_NAME 0x51E340
