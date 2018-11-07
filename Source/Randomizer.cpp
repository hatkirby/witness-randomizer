/*
 * BUGS:
 * Shipwreck vault is solved reversed? -> Not reversed, just "half", you can normally solve orange. Seems to need pattern name.
 * Tutorial sounds don't always play -> Unsure. Not controlled by pattern name.
 * FEATURES:
 * Start the game if it isn't running?
 * Stop swapping colors in desert
 * Look into valid panel swaps for keep walk-ons.
 * Randomize audio logs -- Hard, seem to be unloaded some times?
 * Swap sounds in jungle (along with panels) -- maybe impossible
 * Make orange 7 (all of oranges?) hard. Like big = hard. (See: HARD_MODE)
*/
#include "Memory.h"
#include "Randomizer.h"
#include "Panels.h"
#include "Random.h"
#include <string>
#include <iostream>
#include <numeric>

template <class T>
int find(const std::vector<T> &data, T search, size_t startIndex = 0) {
	for (size_t i=startIndex ; i<data.size(); i++) {
		if (data[i] == search) return i;
	}
	std::cout << "Couldn't find " << search << " in data!" << std::endl;
	exit(-1);
}

bool Randomizer::GameIsRandomized() {
	int currentFrame = GetCurrentFrame();
	if (currentFrame >= _lastRandomizedFrame) {
		// Time went forwards, presumably we're still on the same save
		_lastRandomizedFrame = currentFrame;
		return true;
	}
	// Otherwise, time has gone backwards, so assume new game
	return false;
}

void Randomizer::Randomize()
{
	if (GameIsRandomized()) return;  // Nice sanity check, but should be unnecessary (since Main checks anyways)
	_lastRandomizedFrame = GetCurrentFrame();

	// Content swaps -- must happen before squarePanels
	Randomize(upDownPanels, SWAP_LINES);
	Randomize(leftForwardRightPanels, SWAP_LINES);

	Randomize(squarePanels, SWAP_LINES);

	// Individual area modifications
	RandomizeTutorial();
	RandomizeSymmetry();
	RandomizeDesert();
	RandomizeQuarry();
	RandomizeTreehouse();
	RandomizeKeep();
	RandomizeShadows();
	RandomizeTown();
	RandomizeMonastery();
	RandomizeBunker();
	RandomizeJungle();
	RandomizeSwamp();
	RandomizeMountain();
	// RandomizeChallenge();
	// RandomizeAudioLogs();
}

void Randomizer::AdjustSpeed() {
	// Desert Surface Final Control
	_memory->WritePanelData<float>(0x09F95, OPEN_RATE, {0.04f}); // 4x
	// Swamp Sliding Bridge
	_memory->WritePanelData<float>(0x0061A, OPEN_RATE, {0.1f}); // 4x
	// Mountain 2 Elevator
	_memory->WritePanelData<float>(0x09EEC, OPEN_RATE, {0.075f}); // 3x
}

void Randomizer::RandomizeTutorial() {
	// Disable tutorial cursor speed modifications (not working?)
	_memory->WritePanelData<float>(0x00295, CURSOR_SPEED_SCALE, {1.0});
	_memory->WritePanelData<float>(0x0C373, CURSOR_SPEED_SCALE, {1.0});
	_memory->WritePanelData<float>(0x00293, CURSOR_SPEED_SCALE, {1.0});
	_memory->WritePanelData<float>(0x002C2, CURSOR_SPEED_SCALE, {1.0});
}

void Randomizer::RandomizeSymmetry() {
}

void Randomizer::RandomizeDesert() {
	Randomize(desertPanels, SWAP_LINES);

	// Turn off desert surface 8
	_memory->WritePanelData<float>(0x09F94, POWER, {0.0, 0.0});
	// Turn off desert flood final
	_memory->WritePanelData<float>(0x18076, POWER, {0.0, 0.0});
	// Change desert floating target to desert flood final
	_memory->WritePanelData<int>(0x17ECA, TARGET, {0x18077});
}

void Randomizer::RandomizeQuarry() {
}

void Randomizer::RandomizeTreehouse() {
	// Ensure that whatever pivot panels we have are flagged as "pivotable"
	int panelFlags = _memory->ReadPanelData<int>(0x17DD1, STYLE_FLAGS, 1)[0];
	_memory->WritePanelData<int>(0x17DD1, STYLE_FLAGS, {panelFlags | 0x8000});
	panelFlags = _memory->ReadPanelData<int>(0x17CE3, STYLE_FLAGS, 1)[0];
	_memory->WritePanelData<int>(0x17CE3, STYLE_FLAGS, {panelFlags | 0x8000});
	panelFlags = _memory->ReadPanelData<int>(0x17DB7, STYLE_FLAGS, 1)[0];
	_memory->WritePanelData<int>(0x17DB7, STYLE_FLAGS, {panelFlags | 0x8000});
	panelFlags = _memory->ReadPanelData<int>(0x17E52, STYLE_FLAGS, 1)[0];
	_memory->WritePanelData<int>(0x17E52, STYLE_FLAGS, {panelFlags | 0x8000});
}

void Randomizer::RandomizeKeep() {
}

void Randomizer::RandomizeShadows() {
	// Distance-gate shadows laser to prevent sniping through the bars
	_memory->WritePanelData<float>(0x19650, MAX_BROADCAST_DISTANCE, {2.5});
	// Change the shadows tutorial cable to only activate avoid
	_memory->WritePanelData<int>(0x319A8, CABLE_TARGET_2, {0});
	// Change shadows avoid 8 to power shadows follow
	_memory->WritePanelData<int>(0x1972F, TARGET, {0x1C34C});

	std::vector<int> randomOrder(shadowsPanels.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	RandomizeRange(randomOrder, SWAP_NONE, 0, 8); // Tutorial
	RandomizeRange(randomOrder, SWAP_NONE, 8, 16); // Avoid
	RandomizeRange(randomOrder, SWAP_NONE, 16, 21); // Follow
	ReassignTargets(shadowsPanels, randomOrder);
	// Turn off original starting panel
	_memory->WritePanelData<float>(shadowsPanels[0], POWER, {0.0f, 0.0f});
	// Turn on new starting panel
	_memory->WritePanelData<float>(shadowsPanels[randomOrder[0]], POWER, {1.0f, 1.0f});
}

void Randomizer::RandomizeTown() {
}

void Randomizer::RandomizeMonastery() {
	std::vector<int> randomOrder(monasteryPanels.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	RandomizeRange(randomOrder, SWAP_NONE, 3, 9); // Outer 2 & 3, Inner 1-4
	ReassignTargets(monasteryPanels, randomOrder);
}

void Randomizer::RandomizeBunker() {
	std::vector<int> randomOrder(bunkerPanels.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	// Randomize Tutorial 2-Advanced Tutorial 4 + Glass 1
	// Tutorial 1 cannot be randomized, since no other panel can start on
	// Glass 1 will become door + glass 1, due to the targetting system
	RandomizeRange(randomOrder, SWAP_NONE, 1, 10);
	// Randomize Glass 1-3 into everything after the door/glass 1
	const size_t glass1Index = find(randomOrder, 9);
	RandomizeRange(randomOrder, SWAP_NONE, glass1Index + 1, 12);
	ReassignTargets(bunkerPanels, randomOrder);
}

void Randomizer::RandomizeJungle() {
	std::vector<int> randomOrder(junglePanels.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	// Waves 1 cannot be randomized, since no other panel can start on
	RandomizeRange(randomOrder, SWAP_NONE, 1, 7); // Waves 2-7
	RandomizeRange(randomOrder, SWAP_NONE, 8, 13); // Pitches 1-6
	ReassignTargets(junglePanels, randomOrder);
}

void Randomizer::RandomizeSwamp() {
	// Distance-gate swamp snipe 1 to prevent RNG swamp snipe
	_memory->WritePanelData<float>(0x17C05, MAX_BROADCAST_DISTANCE, {15.0});
}

void Randomizer::RandomizeMountain() {
	// Randomize lasers & some of mountain
	Randomize(lasers, SWAP_TARGETS);
	Randomize(mountainMultipanel, SWAP_LINES);

	// Randomize final pillars order
	std::vector<int> targets = {pillars[0] + 1};
	for (const int pillar : pillars) {
		int target = _memory->ReadPanelData<int>(pillar, TARGET, 1)[0];
		targets.push_back(target);
	}
	targets[5] = pillars[5] + 1;

	std::vector<int> randomOrder(pillars.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	RandomizeRange(randomOrder, SWAP_NONE, 0, 4); // Left Pillars 1-4
	RandomizeRange(randomOrder, SWAP_NONE, 5, 9); // Right Pillars 1-4
	ReassignTargets(pillars, randomOrder, targets);
	// Turn off original starting panels
	_memory->WritePanelData<float>(pillars[0], POWER, {0.0f, 0.0f});
	_memory->WritePanelData<float>(pillars[5], POWER, {0.0f, 0.0f});
	// Turn on new starting panels
	_memory->WritePanelData<float>(pillars[randomOrder[0]], POWER, {1.0f, 1.0f});
	_memory->WritePanelData<float>(pillars[randomOrder[5]], POWER, {1.0f, 1.0f});

	// Read the target of keep front laser, and write it to keep back laser.
	std::vector<int> keepFrontLaserTarget = _memory->ReadPanelData<int>(0x0360E, TARGET, 1);
	_memory->WritePanelData<int>(0x03317, TARGET, keepFrontLaserTarget);
}

void Randomizer::RandomizeChallenge() {
	std::vector<int> randomOrder(challengePanels.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	RandomizeRange(randomOrder, SWAP_NONE, 1, 9); // Easy maze - Triple 2
	std::vector<int> triple1Target = _memory->ReadPanelData<int>(0x00C80, TARGET, 1);
	_memory->WritePanelData<int>(0x00CA1, TARGET, triple1Target);
	_memory->WritePanelData<int>(0x00CB9, TARGET, triple1Target);
	std::vector<int> triple2Target = _memory->ReadPanelData<int>(0x00C22, TARGET, 1);
	_memory->WritePanelData<int>(0x00C59, TARGET, triple2Target);
	_memory->WritePanelData<int>(0x00C68, TARGET, triple2Target);
	ReassignTargets(challengePanels, randomOrder);
}

void Randomizer::RandomizeAudioLogs() {
	std::vector<int> randomOrder(audiologs.size(), 0);
	std::iota(randomOrder.begin(), randomOrder.end(), 0);
	Randomize(randomOrder, SWAP_NONE);
	ReassignNames(audiologs, randomOrder);
}

void Randomizer::Randomize(std::vector<int>& panels, int flags) {
	return RandomizeRange(panels, flags, 0, panels.size());
}

// Range is [start, end)
void Randomizer::RandomizeRange(std::vector<int> &panels, int flags, size_t startIndex, size_t endIndex) {
	if (panels.size() == 0) return;
	if (startIndex >= endIndex) return;
	if (endIndex >= panels.size()) endIndex = panels.size();
	for (size_t i = endIndex-1; i > startIndex; i--) {
		const size_t target = Random::RandInt(startIndex, i);
		if (i != target) {
			// std::cout << "Swapping panels " << std::hex << panels[i] << " and " << std::hex << panels[target] << std::endl;
			SwapPanels(panels[i], panels[target], flags);
			std::swap(panels[i], panels[target]); // Panel indices in the array
		}
	}
}

void Randomizer::SwapPanels(int panel1, int panel2, int flags) {
	std::map<int, int> offsets;

	if (flags & SWAP_TARGETS) {
		offsets[TARGET] = sizeof(int);
	}
	if (flags & SWAP_AUDIO_NAMES) {
		offsets[AUDIO_LOG_NAME] = sizeof(void*);
	}
	if (flags & SWAP_LINES) {
		offsets[PATH_COLOR] = 16;
		offsets[REFLECTION_PATH_COLOR] = 16;
		offsets[DOT_COLOR] = 16;
		offsets[ACTIVE_COLOR] = 16;
		offsets[BACKGROUND_REGION_COLOR] = 12; // Not copying alpha to preserve transparency.
		offsets[SUCCESS_COLOR_A] = 16;
		offsets[SUCCESS_COLOR_B] = 16;
		offsets[STROBE_COLOR_A] = 16;
		offsets[STROBE_COLOR_B] = 16;
		offsets[ERROR_COLOR] = 16;
		offsets[PATTERN_POINT_COLOR] = 16;
		offsets[PATTERN_POINT_COLOR_A] = 16;
		offsets[PATTERN_POINT_COLOR_B] = 16;
		offsets[SYMBOL_A] = 16;
		offsets[SYMBOL_B] = 16;
		offsets[SYMBOL_C] = 16;
		offsets[SYMBOL_D] = 16;
		offsets[SYMBOL_E] = 16;
		offsets[PUSH_SYMBOL_COLORS] = sizeof(int);
		offsets[OUTER_BACKGROUND] = 16;
		offsets[OUTER_BACKGROUND_MODE] = sizeof(int);
		offsets[TRACED_EDGES] = 16;
		offsets[AUDIO_PREFIX] = sizeof(void*);
//		offsets[IS_CYLINDER] = sizeof(int);
//		offsets[CYLINDER_Z0] = sizeof(float);
//		offsets[CYLINDER_Z1] = sizeof(float);
//		offsets[CYLINDER_RADIUS] = sizeof(float);
		offsets[SPECULAR_ADD] = sizeof(float);
		offsets[SPECULAR_POWER] = sizeof(int);
		offsets[PATH_WIDTH_SCALE] = sizeof(float);
		offsets[STARTPOINT_SCALE] = sizeof(float);
		offsets[NUM_DOTS] = sizeof(int);
		offsets[NUM_CONNECTIONS] = sizeof(int);
		offsets[DOT_POSITIONS] = sizeof(void*);
		offsets[DOT_FLAGS] = sizeof(void*);
		offsets[DOT_CONNECTION_A] = sizeof(void*);
		offsets[DOT_CONNECTION_B] = sizeof(void*);
		offsets[DECORATIONS] = sizeof(void*);
		offsets[DECORATION_FLAGS] = sizeof(void*);
		offsets[DECORATION_COLORS] = sizeof(void*);
		offsets[NUM_DECORATIONS] = sizeof(int);
		offsets[REFLECTION_DATA] = sizeof(void*);
		offsets[GRID_SIZE_X] = sizeof(int);
		offsets[GRID_SIZE_Y] = sizeof(int);
		offsets[STYLE_FLAGS] = sizeof(int);
		offsets[SEQUENCE_LEN] = sizeof(int);
		offsets[SEQUENCE] = sizeof(void*);
		offsets[DOT_SEQUENCE_LEN] = sizeof(int);
		offsets[DOT_SEQUENCE] = sizeof(void*);
		offsets[DOT_SEQUENCE_LEN_REFLECTION] = sizeof(int);
		offsets[DOT_SEQUENCE_REFLECTION] = sizeof(void*);
		offsets[NUM_COLORED_REGIONS] = sizeof(int);
		offsets[COLORED_REGIONS] = sizeof(void*);
		offsets[PANEL_TARGET] = sizeof(void*);
		offsets[SPECULAR_TEXTURE] = sizeof(void*);
	}

	for (auto const& [offset, size] : offsets) {
		std::vector<byte> panel1data = _memory->ReadPanelData<byte>(panel1, offset, size);
		std::vector<byte> panel2data = _memory->ReadPanelData<byte>(panel2, offset, size);
		_memory->WritePanelData<byte>(panel2, offset, panel1data);
		_memory->WritePanelData<byte>(panel1, offset, panel2data);
	}
}

void Randomizer::ReassignTargets(const std::vector<int>& panels, const std::vector<int>& order, std::vector<int> targets) {
	if (targets.empty()) {
		// This list is offset by 1, so the target of the Nth panel is in position N (aka the N+1th element)
		// The first panel may not have a wire to power it, so we use the panel ID itself.
		targets = {panels[0] + 1};
		for (const int panel : panels) {
			int target = _memory->ReadPanelData<int>(panel, TARGET, 1)[0];
			targets.push_back(target);
		}
	}

	for (size_t i=0; i<order.size() - 1; i++) {
		// Set the target of order[i] to order[i+1], using the "real" target as determined above.
		const int panelTarget = targets[order[i+1]];
		_memory->WritePanelData<int>(panels[order[i]], TARGET, {panelTarget});
	}
}

void Randomizer::ReassignNames(const std::vector<int>& panels, const std::vector<int>& order) {
	std::vector<int64_t> names;
	for (const int panel : panels) {
		names.push_back(_memory->ReadPanelData<int64_t>(panel, AUDIO_LOG_NAME, 1)[0]);
	}

	for (int i=0; i<panels.size(); i++) {
		_memory->WritePanelData<int64_t>(panels[i], AUDIO_LOG_NAME, {names[order[i]]});
	}
}

short Randomizer::ReadMetadata() {
	return _memory->ReadData<short>({GLOBALS + METADATA}, 1)[0];
}

void Randomizer::WriteMetadata(short metadata) {
	return _memory->WriteData<short>({GLOBALS + METADATA}, {metadata});
}

int Randomizer::GetCurrentFrame() {
	return _memory->ReadData<int>({SCRIPT_FRAMES}, 1)[0];
}
