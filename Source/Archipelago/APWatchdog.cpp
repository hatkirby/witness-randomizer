#include "APWatchdog.h"
#include "..\Quaternion.h"
#include <thread>

void APWatchdog::action() {
	CheckSolvedPanels();
	HandleMovementSpeed();
	HandlePowerSurge();
	UpdateChallengeLock();
}

void APWatchdog::CheckSolvedPanels() {
	std::list<int64_t> solvedLocations;

	if (finalPanel != 0x09F7F && finalPanel != 0xFFF00 && ReadPanelData<int>(finalPanel, SOLVED) && !isCompleted) {
		isCompleted = true;
		ap->StatusUpdate(APClient::ClientStatus::GOAL);
	}
	if (finalPanel == 0x09F7F && !isCompleted)
	{
		float power = ReadPanelData<float>(0x17C34, POWER);

		if (power != 0.0f) {
			isCompleted = true;
			ap->StatusUpdate(APClient::ClientStatus::GOAL);
		}
	}
	if (finalPanel == 0xFFF00 && !isCompleted)
	{
		float power = ReadPanelData<float>(0x17FA2, POWER);

		if (power != 0.0f) {
			isCompleted = true;
			ap->StatusUpdate(APClient::ClientStatus::GOAL);
		}
	}

	auto it = panelIdToLocationId.begin();
	while (it != panelIdToLocationId.end())
	{
		int panelId = it->first;
		int locationId = it->second;

		if (ReadPanelData<int>(panelId, SOLVED))
		{
			solvedLocations.push_back(locationId);

			it = panelIdToLocationId.erase(it);
		}
		else
		{
			it++;
		}
	}

	if (!solvedLocations.empty())
		ap->LocationChecks(solvedLocations);
}

void APWatchdog::MarkLocationChecked(int locationId)
{
	auto it = panelIdToLocationId.begin();
	while (it != panelIdToLocationId.end())
	{
		if (it->second == locationId)
			it = panelIdToLocationId.erase(it);
		else
			it++;
	}
}

void APWatchdog::ApplyTemporarySpeedBoost() {
	temporarySpeedModdificationStartTime = std::chrono::system_clock::now();
	currentSpeed = baseSpeed * 2.0f;
	hasTemporarySpeedModdification = true;
}

void APWatchdog::ApplyTemporarySlow() {
	temporarySpeedModdificationStartTime = std::chrono::system_clock::now();
	currentSpeed = baseSpeed * 0.4f;
	hasTemporarySpeedModdification = true;
}

void APWatchdog::HandleMovementSpeed() {
	if (hasTemporarySpeedModdification && DateTime::since(temporarySpeedModdificationStartTime).count() > 30000) {
		hasTemporarySpeedModdification = false;

		currentSpeed = baseSpeed;
	}

	_memory->WriteMovementSpeed(currentSpeed);
}

void APWatchdog::TriggerPowerSurge() {
	if (hasPowerSurge) {
		powerSurgeStartTime = std::chrono::system_clock::now();
	}
	else {
		hasPowerSurge = true;
		powerSurgeStartTime = std::chrono::system_clock::now();

		for (const auto& panelId : AllPuzzles) {
			if (ReadPanelData<int>(panelId, SOLVED))
				continue;

			std::vector<float> powerValues = ReadPanelData<float>(panelId, POWER, 2);

			if (powerValues[0] > 0) {
				//using -20f as offset to create a unique recogniseable value that is also stored in the save
				WritePanelData<float>(panelId, POWER, { -20.0f + powerValues[0], -20.0f + powerValues[1] });
			}
		}
	}
}

void APWatchdog::HandlePowerSurge() {
	if (hasPowerSurge && DateTime::since(powerSurgeStartTime).count() > 10000)
		ResetPowerSurge();
}

void APWatchdog::ResetPowerSurge() {
	hasPowerSurge = false;

	for (const auto& panelId : AllPuzzles) {
		std::vector<float> powerValues = ReadPanelData<float>(panelId, POWER, 2);

		if (powerValues[0] < -18.0f && powerValues[0] > -22.0f && powerValues[1] < -18.0f && powerValues[1] > -22.0f)
		{
			powerValues[0] -= -20.0f;
			powerValues[1] -= -20.0f;

			WritePanelData<float>(panelId, POWER, powerValues);
			WritePanelData<float>(panelId, NEEDS_REDRAW, { 1 });
		}
	}
}

void APWatchdog::UpdateChallengeLock() {
	float power = ReadPanelData<float>(0x17FA2, POWER); //Challenge is supposed to unlock when the long solution of the box is solved, which activates the Mountain Bottom Layer Discard

	if (power != 0.0f) {
		WritePanelData<float>(0x0A332, POWER, { 1.0, 1.0 });
	}
	else
	{
		WritePanelData<float>(0x0A332, POWER, { 0.0, 0.0 });
	}
}
