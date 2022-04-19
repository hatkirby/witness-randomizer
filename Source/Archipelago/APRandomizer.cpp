#include "APRandomizer.h"

APRandomizer::APRandomizer() {
	_memory = std::make_shared<Memory>("witness64_d3d11.exe");
}

bool APRandomizer::Connect(HWND& messageBoxHandle, std::string& server, std::string& user, std::string& password) {
	std::string uri = buildUri(server);

	ap = new APClient("uuid", "The Witness", uri);

	bool connected = false;
	bool hasConnectionResult = false;

	ap->set_room_info_handler([&]() {
		const int item_handling_flags_all = 7;

		ap->ConnectSlot(user, password, item_handling_flags_all);
	});

	ap->set_location_checked_handler([&](const std::list<int64_t>& locations) {
		for (const auto& locationId : locations) {
			async->MarkLocationChecked(locationId);
		}
	});

	ap->set_slot_connected_handler([&](const nlohmann::json& slotData) {
		Seed = slotData["seed"];
		Hard = slotData["hard_mode"] == true;
		UnlockSymbols = slotData["unlock_symbols"] == true;
		DisableNonRandomizedPuzzles = slotData["disable_non_randomized_puzzles"] == true;

		for (auto& [key, val] : slotData["panelhex_to_id"].items()) {
			int panelId = std::stoul(key, nullptr, 16);
			int locationId = val;

			panelIdToLocationId.insert({ panelId, locationId });
		}

		async = new APWatchdog(ap, panelIdToLocationId);
		async->start();

		connected = true;
		hasConnectionResult = true;
	});

	ap->set_slot_refused_handler([&](const std::list<std::string>& errors) {
		auto errorString = std::accumulate(errors.begin(), errors.end(), std::string(),
			[](const std::string& a, const std::string& b) -> std::string {
				return a + (a.length() > 0 ? "," : "") + b;
			});

		std::wstring wError = std::wstring(errorString.begin(), errorString.end());

		connected = false;
		hasConnectionResult = true;

		WCHAR errorMessage[100] = L"Connection Failed: ";
		wcscat_s(errorMessage, 100, wError.data());

		MessageBox(messageBoxHandle, errorMessage, NULL, MB_OK);
	});

	ap->set_items_received_handler([&](const std::list<APClient::NetworkItem>& items) {
		for (const auto& item : items) {
			switch (item.item) {
				//Puzzle Symbols
				case ITEM_DOTS:							unlockedDots = true;							break;
				case ITEM_COLORED_DOTS:					unlockedColoredDots = true;				break;
				case ITEM_SOUND_DOTS:					unlockedSoundDots = true;					break;
				case ITEM_SYMMETRY:						unlockedSymmetry = true;					break;
				case ITEM_TRIANGLES:						unlockedTriangles = true;					break;
				case ITEM_ERASOR:							unlockedErasers = true;						break;
				case ITEM_TETRIS:							unlockedTetris = true;						break;
				case ITEM_TETRIS_ROTATED:				unlockedTetrisRotated = true;				break;
				case ITEM_TETRIS_NEGATIVE:				unlockedTetrisNegative = true;			break;
				case ITEM_TETRIS_NEGATIVE_ROTATED:	unlockedTetrisNegative = true;			break;
				case ITEM_STARS:							unlockedStars = true;						break;
				case ITEM_STARS_WITH_OTHER_SYMBOL:	unlockedStarsWithOtherSimbol = true;	break;
				case ITEM_B_W_SQUARES:					unlockedStones = true;						break;
				case ITEM_COLORED_SQUARES:				unlockedColoredStones = true;				break;
				case ITEM_SQUARES: unlockedStones = unlockedColoredStones = true;				break;

				//Powerups
				case ITEM_TEMP_SPEED_BOOST:			async->ApplyTemporarySpeedBoost();		break;
				case ITEM_TEMP_SPEED_REDUCTION:		async->ApplyTemporarySlow();				break;

				//Traps
				case ITEM_POWER_SURGE:					async->TriggerPowerSurge();				break;

				default:
					break;
			}

			if (item.item < ITEM_TEMP_SPEED_BOOST) {
				updatePuzzleLocks(item.item);
			}
		}
	});

	(new APServerPoller(ap))->start();

	auto start = std::chrono::system_clock::now();

	while (!hasConnectionResult) {
		if (DateTime::since(start).count() > 5000) { //5 seconnd timeout on waiting for response from server
			connected = false;
			hasConnectionResult = true;

			std::wstring wideServer = Converty::Utf8ToWide(uri);

			WCHAR errorMessage[100] = L"Timeout while connecting to server: ";
			wcscat_s(errorMessage, 100, wideServer.data());

			MessageBox(messageBoxHandle, errorMessage, NULL, MB_OK);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	return connected;
}

std::string APRandomizer::buildUri(std::string& server)
{
	std::string uri = server;

	if (uri.rfind("ws://", 0) == std::string::npos)
		uri = "ws://" + uri;
	if (uri.find(":") == std::string::npos)
		uri = uri + ":38281";
	else if (uri.rfind(":") == uri.length())
		uri = uri + "38281";

	return uri;
}

void APRandomizer::Initialize(HWND loadingHandle) {
	PreventSnipes(); //Prevents Snipes to preserve progression randomizer experience

	if (UnlockSymbols)
		disablePuzzles(loadingHandle);

	if (DisableNonRandomizedPuzzles)
		; //TODO: handle
}

void APRandomizer::disablePuzzles(HWND loadingHandle) {
	for (int i = 0; i < sizeof(AllPuzzles) / sizeof(AllPuzzles[0]); i++)	{
		std::wstring text = L"Locking puzzles: " + std::to_wstring(i) + L"/" + std::to_wstring(sizeof(AllPuzzles));
		SetWindowText(loadingHandle, text.c_str());

		if (!_memory->ReadPanelData<int>(AllPuzzles[i], SOLVED))
			updatePuzzleLock(AllPuzzles[i]);
	}

	SetWindowText(loadingHandle, L"Done!");
}

void APRandomizer::updatePuzzleLocks(int itemIndex) {
	std::vector<PuzzleData*> puzzlesToUpdate;

	for (auto const& [id, puzzle] : disabledPuzzles) {
		switch (itemIndex) {
			//Puzzle Symbols
			case ITEM_DOTS:							if (puzzle->hasDots) puzzlesToUpdate.push_back(puzzle);							break;
			case ITEM_COLORED_DOTS:					if (puzzle->hasColoredDots) puzzlesToUpdate.push_back(puzzle);					break;
			case ITEM_SOUND_DOTS:					if (puzzle->hasSoundDots) puzzlesToUpdate.push_back(puzzle);					break;
			case ITEM_INVISIBLE_DOTS:				if (puzzle->hasInvisibleDots) puzzlesToUpdate.push_back(puzzle);				break;
			case ITEM_SYMMETRY:						if (puzzle->hasSymmetry) puzzlesToUpdate.push_back(puzzle);						break;
			case ITEM_TRIANGLES:						if (puzzle->hasTriangles) puzzlesToUpdate.push_back(puzzle);					break;
			case ITEM_ERASOR:							if (puzzle->hasErasers) puzzlesToUpdate.push_back(puzzle);						break;
			case ITEM_TETRIS:							if (puzzle->hasTetris) puzzlesToUpdate.push_back(puzzle);						break;
			case ITEM_TETRIS_ROTATED:				if (puzzle->hasTetrisRotated) puzzlesToUpdate.push_back(puzzle);				break;
			case ITEM_TETRIS_NEGATIVE:				if (puzzle->hasTetrisNegative) puzzlesToUpdate.push_back(puzzle);				break;
			case ITEM_TETRIS_NEGATIVE_ROTATED:	if (puzzle->hasTetrisNegativeRotated) puzzlesToUpdate.push_back(puzzle);	break;
			case ITEM_STARS:							if (puzzle->hasStars) puzzlesToUpdate.push_back(puzzle);							break;
			case ITEM_STARS_WITH_OTHER_SYMBOL:	if (puzzle->hasStarsWithOtherSymbol) puzzlesToUpdate.push_back(puzzle);		break;
			case ITEM_B_W_SQUARES:					if (puzzle->hasStones) puzzlesToUpdate.push_back(puzzle);						break;
			case ITEM_COLORED_SQUARES:				if (puzzle->hasColoredStones) puzzlesToUpdate.push_back(puzzle);				break;
			case ITEM_SQUARES: if (puzzle->hasStones || puzzle->hasColoredStones) puzzlesToUpdate.push_back(puzzle);			break;

			default:																																			break;
		}
	}

	for (auto& puzzle : puzzlesToUpdate) {
		updatePuzzleLock(puzzle->id);
	}
}

void APRandomizer::GenerateNormal() {
#if _DEBUG
	_memory->WritePanelData<float>(0x0001B, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x0001B, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x012C9, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x012C9, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x0001C, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x0001C, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x0001D, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x0001D, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x0001E, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x0001E, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x0001F, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x0001F, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x00020, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x00020, NEEDS_REDRAW, { 1 });
	_memory->WritePanelData<float>(0x00021, POWER, { 1.0f, 1.0f });
	_memory->WritePanelData<int>(0x00021, NEEDS_REDRAW, { 1 });
#endif
}

void APRandomizer::GenerateHard() {
}

void APRandomizer::PreventSnipes()
{
	// Distance-gate shadows laser to prevent sniping through the bars
	_memory->WritePanelData<float>(0x19650, MAX_BROADCAST_DISTANCE, { 2.5 });
}

void APRandomizer::updatePuzzleLock(int id) {
	bool isLocked;
	PuzzleData* puzzle;

	if (disabledPuzzles.count(id) == 1) {
		isLocked = true;
		puzzle = disabledPuzzles[id];
	}
	else {
		isLocked = false;
		puzzle = new PuzzleData(id);
		puzzle->Read(_memory);
	}

	if ((puzzle->hasStones && !unlockedStones)
		|| (puzzle->hasStars && !unlockedStars)
		|| (puzzle->hasStarsWithOtherSymbol && !unlockedStarsWithOtherSimbol)
		|| (puzzle->hasTetris && !unlockedTetris)
		|| (puzzle->hasTetrisRotated && !unlockedTetrisRotated)
		|| (puzzle->hasTetrisNegative && !unlockedTetrisNegative)
		|| (puzzle->hasTetrisNegative && !unlockedTetrisNegative)
		|| (puzzle->hasTetrisNegativeRotated && !unlockedTetrisNegativeRotated)
		|| (puzzle->hasErasers && !unlockedErasers)
		|| (puzzle->hasTriangles && !unlockedTriangles)
		|| (puzzle->hasDots && !unlockedDots)
		|| (puzzle->hasColoredDots && !unlockedColoredDots)
		//|| (puzzle->hasInvisibleDots && !unlockedInvisibleDots) //NYI
		|| (puzzle->hasSoundDots && !unlockedSoundDots)
		|| (puzzle->hasArrows && !unlockedArrows)
		|| (puzzle->hasSymmetry && !unlockedSymmetry))
	{
		//puzzle should be locked
		if (!isLocked)
			disabledPuzzles.insert({ id, puzzle });

		const int width = 4;
		const int height = 2;

		std::vector<float> intersections;
		std::vector<int> intersectionFlags;
		std::vector<int> connectionsA;
		std::vector<int> connectionsB;
		std::vector<int> decorations;
		std::vector<int> decorationsFlags;
		std::vector<int> polygons;

		addMissingSimbolsDisplay(intersections, intersectionFlags, connectionsA, connectionsB);

		addPuzzleSimbols(puzzle, intersections, intersectionFlags, connectionsA, connectionsB, decorations, decorationsFlags, polygons);

		createText(id, "missing", intersections, intersectionFlags, connectionsA, connectionsB, 0.1f, 0.9f, 0.1f, 0.4f);

		_memory->WritePanelData<int>(id, GRID_SIZE_X, { width + 1 });
		_memory->WritePanelData<int>(id, GRID_SIZE_Y, { height + 1 });
		_memory->WritePanelData<float>(id, PATH_WIDTH_SCALE, { 0.5f });
		_memory->WritePanelData<int>(id, NUM_DOTS, { static_cast<int>(intersectionFlags.size()) }); //amount of intersections
		_memory->WriteArray<float>(id, DOT_POSITIONS, intersections); //position of each point as array of x,y,x,y,x,y so this vector is twice the suze if sourceIntersectionFlags
		_memory->WriteArray<int>(id, DOT_FLAGS, intersectionFlags); //flags for each point such as entrance or exit
		_memory->WritePanelData<int>(id, NUM_CONNECTIONS, { static_cast<int>(connectionsA.size()) }); //amount of connected points, for each connection we specify start in sourceConnectionsA and end in sourceConnectionsB
		_memory->WriteArray<int>(id, DOT_CONNECTION_A, connectionsA); //start of a connection between points, contains position of point in sourceIntersectionFlags
		_memory->WriteArray<int>(id, DOT_CONNECTION_B, connectionsB); //end of a connection between points, contains position of point in sourceIntersectionFlags
		_memory->WritePanelData<int>(id, NUM_DECORATIONS, { static_cast<int>(decorations.size()) });
		_memory->WriteArray<int>(id, DECORATIONS, decorations);
		_memory->WriteArray<int>(id, DECORATION_FLAGS, decorationsFlags);
		if (polygons.size() > 0) { //TODO maybe just always write ?
			_memory->WritePanelData<int>(id, NUM_COLORED_REGIONS, { static_cast<int>(polygons.size()) / 4 }); //why devide by 4 tho?
			_memory->WriteArray<int>(id, COLORED_REGIONS, polygons);
		}
		_memory->WritePanelData<int>(id, NEEDS_REDRAW, { 1 });
	}
	else if (isLocked) {
		//puzzle is locked but should nolonger be locked
		puzzle->Restore(_memory);
		disabledPuzzles.erase(puzzle->id);
		delete puzzle;
	}
	else {
		//puzzle isnt locked and should not be locked
		delete puzzle;
	}
}

void APRandomizer::addMissingSimbolsDisplay(std::vector<float>& intersections, std::vector<int>& intersectionFlags, std::vector<int>& connectionsA, std::vector<int>& connectionsB) {
	//does not respect width/height but its hardcoded at a grid of 4/2

	//panel coordinates go from 0,0 in bottom left to 1,1 in top right
	//format vector of x,y,x,y,x,y,x,y,x,y
	std::vector<float> gridIntersections = {
		0.1f, 0.1f, 0.3f, 0.1f, 0.5f, 0.1f, 0.7f, 0.1f, 0.9f, 0.1f, //bottom row
		0.1f, 0.3f, 0.3f, 0.3f, 0.5f, 0.3f, 0.7f, 0.3f, 0.9f, 0.3f, //middle row
		0.1f, 0.5f, 0.3f, 0.5f, 0.5f, 0.5f, 0.7f, 0.5f, 0.9f, 0.5f,  //top row
		0.0f, 0.0f, 1.0f, 1.0f //start, exit needed to prevent crash on re-randomization
	};

	//flag for each point
	//must be half the size of gridIntersections as its one per intersection
	std::vector<int> gridIntersectionFlags = {
		0, 0, 0, 0, 0, //bottom row
		0, 0, 0, 0, 0, //middle row
		0, 0, 0, 0, 0, //top row
		IntersectionFlags::STARTPOINT, IntersectionFlags::ENDPOINT //start, exit
	};

	//starts of a connection, the value refers to the position of an intersection point the intersections array
	//works together with gridConnectionsB
	std::vector<int> gridConnectionsA = {
		0, 1, 2, 3, //horizontal bottom row
		5, 6, 7, 8, //horizontal middle row
		10,11,12,13,//horizontal top row
		0, 5, //vertical left column
		1, 6, //vertical 2 column
		2, 7, //vertical 3 column
		3, 8, //vertical 4 column
		4, 9  //vertical right column
	};

	//end of a connection, the value refers to the position of an intersection point the intersections array
	//works together with gridConnectionsA
	std::vector<int> gridConnectionsB = {
		1, 2, 3, 4, //horizontal bottom row
		6, 7, 8, 9, //horizontal middle row
		11,12,13,14,//horizontal top row
		5, 10, //vertical left column
		6, 11, //vertical 2 column
		7, 12, //vertical 3 column
		8, 13, //vertical 4 column
		9, 14  //vertical right column
	};

	intersections.insert(intersections.begin(), gridIntersections.begin(), gridIntersections.end());
	intersectionFlags.insert(intersectionFlags.begin(), gridIntersectionFlags.begin(), gridIntersectionFlags.end());
	connectionsA.insert(connectionsA.begin(), gridConnectionsA.begin(), gridConnectionsA.end());
	connectionsB.insert(connectionsB.begin(), gridConnectionsB.begin(), gridConnectionsB.end());
}

void APRandomizer::createText(int id, std::string text, std::vector<float>& intersections, std::vector<int>& intersectionFlags, 
	std::vector<int>& connectionsA, std::vector<int>& connectionsB, float left, float right, float top, float bottom) {

	int currentIntersections = intersections.size();

	Special::createText(id, text, intersections, connectionsA, connectionsB, 0.1f, 0.9f, 0.1f, 0.4f);

	int newIntersections = intersections.size();

	for (int i = 0; i < (newIntersections - currentIntersections) / 2; i++) {
		intersectionFlags.emplace_back(0);
	}
}

void APRandomizer::addPuzzleSimbols(PuzzleData* puzzle, 
	std::vector<float>& intersections, std::vector<int>& intersectionFlags, std::vector<int>& connectionsA, std::vector<int>& connectionsB,
	std::vector<int>& decorations, std::vector<int>& decorationsFlags, std::vector<int>& polygons) {

	//does not respect width/height but its hardcoded at a grid of 4/2
	//stored the puzzle simnbols per grid section, from bottom to top from left to right so buttom left is decoration 0
	std::vector<int> gridDecorations(8, 0);
	std::vector<int> gridDecorationsFlags(8, 0);

	int column = 4;
	int secondRowOffset = -column;

	if (puzzle->hasStones && !unlockedStones && puzzle->hasColoredStones && !unlockedColoredStones)	{
		gridDecorations[column] = Decoration::Stone | Decoration::Color::Black;
		gridDecorations[column + secondRowOffset] = Decoration::Stone | Decoration::Color::Blue;
		column++;
	}
	else if (puzzle->hasColoredStones && !unlockedColoredStones) {
		gridDecorations[column] = Decoration::Stone | Decoration::Color::Orange;
		gridDecorations[column + secondRowOffset] = Decoration::Stone | Decoration::Color::Blue;
		column++;
	}
	else if (puzzle->hasStones && !unlockedStones) {
		gridDecorations[column] = Decoration::Stone | Decoration::Color::Black;
		gridDecorations[column + secondRowOffset] = Decoration::Stone | Decoration::Color::White;
		column++;
	}

	if (puzzle->hasStarsWithOtherSymbol && !unlockedStarsWithOtherSimbol) {
		gridDecorations[column] = Decoration::Star | Decoration::Color::Green;
		gridDecorations[column + secondRowOffset] = Decoration::Stone | Decoration::Color::Green;
		column++;
	} else if (puzzle->hasStars && !unlockedStars) {
		gridDecorations[column] = Decoration::Star | Decoration::Color::Magenta;
		column++;
	}

	const int defaultLShape = 0x00170000;
	if (	(puzzle->hasTetris && !unlockedTetris)
		|| (puzzle->hasTetrisRotated && !unlockedTetrisRotated) 
		|| (puzzle->hasTetrisNegative && !unlockedTetrisNegative) 
		|| (puzzle->hasTetrisNegativeRotated && !unlockedTetrisNegativeRotated))
	{
		if ((puzzle->hasTetris && !unlockedTetris) && (puzzle->hasTetrisRotated && !unlockedTetrisRotated))
			gridDecorations[column] = 
				Decoration::Poly | Decoration::Color::Yellow | defaultLShape | Decoration::Can_Rotate;
		else if (puzzle->hasTetris && !unlockedTetris)
			gridDecorations[column] =
				Decoration::Poly | Decoration::Color::Yellow | defaultLShape;

		if ((puzzle->hasTetrisNegative && !unlockedTetrisNegative) && (puzzle->hasTetrisNegativeRotated && !unlockedTetrisNegativeRotated))
			gridDecorations[column + secondRowOffset] = 
				Decoration::Poly | Decoration::Color::Blue | defaultLShape | Decoration::Negative | Decoration::Can_Rotate;
		else if (puzzle->hasTetrisNegative && !unlockedTetrisNegative)
			gridDecorations[column + secondRowOffset] =
				Decoration::Poly | Decoration::Color::Blue | defaultLShape | Decoration::Negative;

		column++;
	}

	if (puzzle->hasErasers && !unlockedErasers) {
		gridDecorations[column] = Decoration::Eraser;
		column++;
	}

	if (puzzle->hasTriangles && !unlockedTriangles)	{
		gridDecorations[column] = Decoration::Triangle2;
		column++;
	}

	if (puzzle->hasColoredDots && !unlockedColoredDots) {
		intersectionFlags[11] = IntersectionFlags::DOT | IntersectionFlags::DOT_IS_BLUE;
		intersectionFlags[12] = IntersectionFlags::DOT | IntersectionFlags::DOT_IS_ORANGE;

		if (puzzle->hasDots && !unlockedDots)
			intersectionFlags[13] = IntersectionFlags::DOT;
	}
	else if (puzzle->hasInvisibleDots && !unlockedInvisibleDots) {
		intersectionFlags[11] = IntersectionFlags::DOT | IntersectionFlags::DOT_IS_INVISIBLE;
		intersectionFlags[12] = IntersectionFlags::DOT;
	}
	else if (puzzle->hasSoundDots && !unlockedSoundDots) {
		intersectionFlags[11] = IntersectionFlags::DOT | IntersectionFlags::DOT_SMALL;
		intersectionFlags[12] = IntersectionFlags::DOT | IntersectionFlags::DOT_MEDIUM;
		intersectionFlags[13] = IntersectionFlags::DOT | IntersectionFlags::DOT_LARGE;
	}
	else if (puzzle->hasDots && !unlockedDots) {
		intersectionFlags[11] = IntersectionFlags::DOT;
	}

	if (puzzle->hasSymmetry && !unlockedSymmetry) {
		float x = intersections[column - 4];
		int intersectionOffset = intersectionFlags.size();

		std::vector<float> symmetryIntersections = {
			x + 0.05f, 0.35f, x + 0.08f, 0.35f, x + 0.12f, 0.35f, x + 0.15f, 0.35f, //bottom row
			x + 0.05f, 0.40f, x + 0.08f, 0.40f, x + 0.12f, 0.40f, x + 0.15f, 0.40f, //middle row
			x + 0.05f, 0.45f, x + 0.08f, 0.45f, x + 0.12f, 0.45f, x + 0.15f, 0.45f, //top row
		};
		std::vector<int> symmetryIntersectionFlags = {
			0, 0, 0, 0, //bottom row
			0, 0, 0, 0, //middle row
			0, 0, 0, 0, //top row
		};
		std::vector<int> symmetryConnectionsA = {
			intersectionOffset + 0, intersectionOffset + 2, //horizontal bottom row
			intersectionOffset + 4, intersectionOffset + 6, //horizontal middle row
			intersectionOffset + 8, intersectionOffset + 11,//horizontal top row
			intersectionOffset + 4, // vertical left column
			intersectionOffset + 1, // vertical 2 column
			intersectionOffset + 2, // vertical 3 column
			intersectionOffset + 7, // vertical right column
		};
		std::vector<int> symmetryConnectionsB = {
			intersectionOffset + 1, intersectionOffset + 3, //horizontal bottom row
			intersectionOffset + 5, intersectionOffset + 7, //horizontal middle row
			intersectionOffset + 9, intersectionOffset + 10,//horizontal top row
			intersectionOffset + 8, // vertical left column
			intersectionOffset + 5, // vertical 2 column
			intersectionOffset + 6, // vertical 3 column
			intersectionOffset + 11,// vertical right column
		};

		intersections.insert(intersections.end(), symmetryIntersections.begin(), symmetryIntersections.end());
		intersectionFlags.insert(intersectionFlags.end(), symmetryIntersectionFlags.begin(), symmetryIntersectionFlags.end());
		connectionsA.insert(connectionsA.end(), symmetryConnectionsA.begin(), symmetryConnectionsA.end());
		connectionsB.insert(connectionsB.end(), symmetryConnectionsB.begin(), symmetryConnectionsB.end());
	}

	if (puzzle->hasArrows && !unlockedArrows)	{
		//TODO Fix meh
		Panel panel;
		panel.render_arrow(column, 1, 2, 0, intersections, intersectionFlags, polygons);
		column++;
	}

	decorations.insert(decorations.begin(), gridDecorations.begin(), gridDecorations.end());
	decorationsFlags.insert(decorationsFlags.begin(), gridDecorationsFlags.begin(), gridDecorationsFlags.end());
}