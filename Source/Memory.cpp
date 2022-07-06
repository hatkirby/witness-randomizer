// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "Memory.h"
#include "Memoryapi.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>

#undef PROCESSENTRY32
#undef Process32Next

Memory::Memory(const std::string& processName) {
	std::string process32 = "witness_d3d11.exe";

	// First, get the handle of the process
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	while (Process32Next(snapshot, &entry)) {
		if (processName == entry.szExeFile) {
			_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
			break;
		}
	}
	if (!_handle) {
		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(entry);
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		while (Process32Next(snapshot, &entry)) {
			if (process32 == entry.szExeFile) {
				MessageBox(GetActiveWindow(), L"You appear to be running the 32 bit version of The Witness. Please run the 64 bit version instead.", NULL, MB_OK);
				throw std::exception("Unable to find process!");
			}
		}

		MessageBox(GetActiveWindow(), L"Process not found in RAM. Please open The Witness and then try again.", NULL, MB_OK);
		throw std::exception("Unable to find process!");
	}

	// Next, get the process base address
	DWORD numModules;
	std::vector<HMODULE> moduleList(1024);
	EnumProcessModulesEx(_handle, &moduleList[0], static_cast<DWORD>(moduleList.size()), &numModules, 3);

	std::string name(64, '\0');
	for (DWORD i = 0; i < numModules / sizeof(HMODULE); i++) {
		int length = GetModuleBaseNameA(_handle, moduleList[i], &name[0], static_cast<DWORD>(name.size()));
		name.resize(length);
		if (processName == name) {
			_baseAddress = (uintptr_t)moduleList[i];
			break;
		}
	}
	if (_baseAddress == 0) {
		throw std::exception("Couldn't find the base process address!");
	}
}

Memory::~Memory() {
	CloseHandle(_handle);
}

// Copied from Witness Trainer https://github.com/jbzdarkid/witness-trainer/blob/master/Source/Memory.cpp#L218
int find(const std::vector<byte> &data, const std::vector<byte> &search) {
	const byte* dataBegin = &data[0];
	const byte* searchBegin = &search[0];
	size_t maxI = data.size() - search.size();
	size_t maxJ = search.size();

	for (int i=0; i<maxI; i++) {
		bool match = true;
		for (size_t j=0; j<maxJ; j++) {
			if (*(dataBegin + i + j) == *(searchBegin + j)) {
				continue;
			}
			match = false;
			break;
		}
		if (match) return i;
	}
	return -1;
}

int Memory::findGlobals() {
	const std::vector<byte> scanBytes = {0x74, 0x41, 0x48, 0x85, 0xC0, 0x74, 0x04, 0x48, 0x8B, 0x48, 0x10};
	#define BUFFER_SIZE 0x10000 // 10 KB
	std::vector<byte> buff;
	buff.resize(BUFFER_SIZE + 0x100); // padding in case the sigscan is past the end of the buffer

	for (uintptr_t i = 0; i < 0x500000; i += BUFFER_SIZE) {
		SIZE_T numBytesWritten;
		if (!ReadProcessMemory(_handle, reinterpret_cast<void*>(_baseAddress + i), &buff[0], buff.size(), &numBytesWritten)) continue;
		buff.resize(numBytesWritten);
		int index = find(buff, scanBytes);
		if (index == -1) continue;

		index = index + 0x14; // This scan targets a line slightly before the key instruction
		// (address of next line) + (index interpreted as 4byte int)
		Memory::GLOBALS = (int)(i + index + 4) + *(int*)&buff[index];
		break;
	}

	return Memory::GLOBALS;
}

void Memory::findMovementSpeed() {
	executeSigScan({ 0xF3, 0x0F, 0x59, 0xFD, 0xF3, 0x0F, 0x5C, 0xC8 }, [this](__int64 offset, int index, const std::vector<byte>& data) {
		int found = 0;
		// This doesn't have a consistent offset from the scan, so search until we find "jmp +08"
		for (; index < data.size(); index++) {
			if (data[index - 2] == 0xEB && data[index - 1] == 0x08) {
				this->ACCELERATION = Memory::ReadStaticInt(offset, index - 0x06, data);
				this->DECELERATION = Memory::ReadStaticInt(offset, index + 0x04, data);
				found++;
				break;
			}
		}

		// Once again, there's no consistent offset, so we read until "movss xmm1, [addr]"
		for (; index < data.size(); index++) {
			if (data[index - 4] == 0xF3 && data[index - 3] == 0x0F && data[index - 2] == 0x10 && data[index - 1] == 0x0D) {
				this->RUNSPEED = Memory::ReadStaticInt(offset, index, data);
				found++;
				break;
			}
		}
		return (found == 2);
		});
}

void Memory::findActivePanel() {
	executeSigScan({ 0xF2, 0x0F, 0x58, 0xC8, 0x66, 0x0F, 0x5A, 0xC1, 0xF2 }, [this](__int64 offset, int index, const std::vector<byte>& data) {
		this->ACTIVEPANELOFFSETS = {};
		this->ACTIVEPANELOFFSETS.push_back(Memory::ReadStaticInt(offset, index + 0x36, data, 5));
		this->ACTIVEPANELOFFSETS.push_back(data[index + 0x5A]); // This is 0x10 in both versions I have, but who knows.

		this->ACTIVEPANELOFFSETS.push_back(*(int*)&data[index + 0x54]);

		return true;
	});
}

int Memory::GetActivePanel() {
	return this->ReadData<int>(this->ACTIVEPANELOFFSETS, 1)[0] - 1;
}

__int64 Memory::ReadStaticInt(__int64 offset, int index, const std::vector<byte>& data, size_t bytesToEOL) {
	// (address of next line) + (index interpreted as 4byte int)
	return offset + index + bytesToEOL + *(int*)&data[index];
}

#define BUFFER_SIZE 0x10000 // 10 KB
void Memory::executeSigScan(const std::vector<byte>& scanBytes, const ScanFunc2& scanFunc) {
	std::vector<byte> buff;
	buff.resize(BUFFER_SIZE + 0x100); // padding in case the sigscan is past the end of the buffer

	for (uintptr_t i = _baseAddress; i < _baseAddress + 0x500000; i += BUFFER_SIZE) {
		SIZE_T numBytesWritten;
		if (!ReadProcessMemory(_handle, reinterpret_cast<void*>(i), &buff[0], buff.size(), &numBytesWritten)) continue;
		buff.resize(numBytesWritten);
		int index = find(buff, scanBytes);
		if (index == -1) continue;
		scanFunc(i - _baseAddress, index, buff); // We're expecting i to be relative to the base address here.
		return;
	}
}

void Memory::ThrowError(std::string message) {
	if (!showMsg) {
		if(errorWindow != NULL){
			time_t now = time(NULL);
			//char *str = asctime(localtime(&now));
			tm now_tm = {};
			char str[26] = {};
			localtime_s(&now_tm, &now);
			asctime_s(str, 26, &now_tm);
			std::string str_r(str);
			SetWindowText(errorWindow, (L"Most recent error on " + std::wstring(str_r.begin(), str_r.end()) + L"\n" + std::wstring(message.begin(), message.end())).c_str());
		}
		throw std::exception(message.c_str());
	}
	DWORD exitCode;
	GetExitCodeProcess(_handle, &exitCode);
	if (exitCode != STILL_ACTIVE) throw std::exception(message.c_str());
	message += "\nPlease close The Witness and try again. If the error persists, please report the issue on the Github Issues page.";
	MessageBoxA(GetActiveWindow(), message.c_str(), NULL, MB_OK);
	throw std::exception(message.c_str());
}

void Memory::ThrowError(const std::vector<int>& offsets, bool rw_flag) {
	std::stringstream ss; ss << std::hex;
	if (offsets.size() == 4) {
		ss << "Error " << (rw_flag ? "writing" : "reading") << " 0x" << offsets[3] << " in panel 0x" << offsets[2] / 8;
		ThrowError(ss.str());
	}
	else if (offsets.size() == 3) {
		for (int i : offsets) ss << "0x" << i << " ";
		ThrowError("Error computing offsets: " + ss.str());
	}
	else {
		for (int i : offsets) ss << "0x" << i << " ";
		ThrowError("Unknown error: " + ss.str());
	}
}

void Memory::ThrowError() {
	std::string message(256, '\0');
	int length = FormatMessageA(4096, nullptr, GetLastError(), 1024, &message[0], static_cast<DWORD>(message.size()), nullptr);
	message.resize(length);
	ThrowError(message);
}

void* Memory::ComputeOffset(std::vector<int> offsets)
{
	// Leave off the last offset, since it will be either read/write, and may not be of type unitptr_t.
	int final_offset = offsets.back();
	offsets.pop_back();

	uintptr_t cumulativeAddress = _baseAddress;
	for (const int offset : offsets) {
		cumulativeAddress += offset;

		const auto search = _computedAddresses.find(cumulativeAddress);
		if (search == std::end(_computedAddresses)) {
			// If the address is not yet computed, then compute it.
			uintptr_t computedAddress = 0;
			if (!Read(reinterpret_cast<LPVOID>(cumulativeAddress), &computedAddress, sizeof(uintptr_t))) {
				ThrowError(offsets, false);
			}
			_computedAddresses[cumulativeAddress] = computedAddress;
		}

		cumulativeAddress = _computedAddresses[cumulativeAddress];
	}
	return reinterpret_cast<void*>(cumulativeAddress + final_offset);
}

void Memory::CallVoidFunction(int id, uint64_t functionAdress) {
	std::lock_guard<std::recursive_mutex> lock(mtx);

	uint64_t offset = reinterpret_cast<uintptr_t>(ComputeOffset({ GLOBALS, 0x18, id * 8, 0 }));

	unsigned char buffer[] =
		"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00" //mov rax [address]
		"\x48\xB9\x00\x00\x00\x00\x00\x00\x00\x00" //mov rcx [address]
		"\x48\x83\xEC\x48" // sub rsp,48
		"\xFF\xD0" //call rax
		"\x48\x83\xC4\x48" // add rsp,48
		"\xC3"; //ret

	buffer[2] = functionAdress & 0xff; //address of laser activation function
	buffer[3] = (functionAdress >> 8) & 0xff;
	buffer[4] = (functionAdress >> 16) & 0xff;
	buffer[5] = (functionAdress >> 24) & 0xff;
	buffer[6] = (functionAdress >> 32) & 0xff;
	buffer[7] = (functionAdress >> 40) & 0xff;
	buffer[8] = (functionAdress >> 48) & 0xff;
	buffer[9] = (functionAdress >> 56) & 0xff;
	buffer[12] = offset & 0xff; //address of laser
	buffer[13] = (offset >> 8) & 0xff;
	buffer[14] = (offset >> 16) & 0xff;
	buffer[15] = (offset >> 24) & 0xff;
	buffer[16] = (offset >> 32) & 0xff;
	buffer[17] = (offset >> 40) & 0xff;
	buffer[18] = (offset >> 48) & 0xff;
	buffer[19] = (offset >> 56) & 0xff;

	SIZE_T allocation_size = sizeof(buffer);

	LPVOID allocation_start = VirtualAllocEx(_handle, NULL, allocation_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(_handle, allocation_start, buffer, allocation_size, NULL);
	CreateRemoteThread(_handle, NULL, 0, (LPTHREAD_START_ROUTINE)allocation_start, NULL, 0, 0);
}

void Memory::DisplayHudMessage(std::string message) {
	char buffer[1024];

	if (!_messageAddress) {
		_messageAddress = VirtualAllocEx(_handle, NULL, sizeof(buffer), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		__int64 address = 0x1401E9E6C;
		LPVOID addressPointer = reinterpret_cast<LPVOID>(address);
		__int32 addressOf8 = 0x0032BEC8;

		Write(addressPointer, &addressOf8, sizeof(addressOf8));
	}

	strcpy_s(buffer, message.c_str());

	WriteProcessMemory(_handle, _messageAddress, buffer, sizeof(buffer), NULL);




	__int64 funcAdress = 0x1401E9E30;
	__int64 messageAddress = reinterpret_cast<__int64>(_messageAddress);

	unsigned char asmBuff[] =
		"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00" //mov rax [address]
		"\x48\xB9\x00\x00\x00\x00\x00\x00\x00\x00" //mov rcx [address]
		"\x48\x83\xEC\x48" // sub rsp,48
		"\xFF\xD0" //call rax
		"\x48\x83\xC4\x48" // add rsp,48
		"\xC3"; //ret

	asmBuff[2] = funcAdress & 0xff;
	asmBuff[3] = (funcAdress >> 8) & 0xff;
	asmBuff[4] = (funcAdress >> 16) & 0xff;
	asmBuff[5] = (funcAdress >> 24) & 0xff;
	asmBuff[6] = (funcAdress >> 32) & 0xff;
	asmBuff[7] = (funcAdress >> 40) & 0xff;
	asmBuff[8] = (funcAdress >> 48) & 0xff;
	asmBuff[9] = (funcAdress >> 56) & 0xff;
	asmBuff[12] = messageAddress & 0xff;
	asmBuff[13] = (messageAddress >> 8) & 0xff;
	asmBuff[14] = (messageAddress >> 16) & 0xff;
	asmBuff[15] = (messageAddress >> 24) & 0xff;
	asmBuff[16] = (messageAddress >> 32) & 0xff;
	asmBuff[17] = (messageAddress >> 40) & 0xff;
	asmBuff[18] = (messageAddress >> 48) & 0xff;
	asmBuff[19] = (messageAddress >> 56) & 0xff;

	SIZE_T allocation_size = sizeof(asmBuff);

	LPVOID allocation_start = VirtualAllocEx(_handle, NULL, allocation_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(_handle, allocation_start, asmBuff, allocation_size, NULL);
	CreateRemoteThread(_handle, NULL, 0, (LPTHREAD_START_ROUTINE)allocation_start, NULL, 0, 0);
}

int Memory::GLOBALS = 0;
int Memory::RUNSPEED = 0;
int Memory::ACCELERATION = 0;
int Memory::DECELERATION = 0;
std::vector<int> Memory::ACTIVEPANELOFFSETS = {};
bool Memory::showMsg = false;
HWND Memory::errorWindow = NULL;
int Memory::globalsTests[3] = {
	0x62D0A0, //Steam and Epic Games
	0x62B0A0, //Good Old Games
	0x5B28C0 //Older Versions
};
