//
//  MemoryLoader.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 17/01/2024.
//

#include "MemoryLoader.h"

#include <fstream>
#include <iostream>
#include "../MemoryManager.h"
#include "ImGuiFileDialog.h"
#include <locale.h>

bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank, size_t fileSize) {
	bool res = false;
	
	uint8_t* pMem;
	if (bAuxBank)
		pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + position;
	else
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + position;
	
	std::ifstream file(filePath, std::ios::binary);
	if (file) {
		// Move to the end to get the file size
		if (fileSize == 0) {
			file.seekg(0, std::ios::end);
			fileSize = file.tellg();
		}
		
		if (fileSize == 0) {
			std::cerr << "Error: File size is zero." << std::endl;
		} else {
			if ((position + fileSize) > (_A2_MEMORY_SHADOW_END))
				fileSize = _A2_MEMORY_SHADOW_END - position;
			file.seekg(0, std::ios::beg); // Go back to the start of the file
			file.read(reinterpret_cast<char*>(pMem), fileSize);
			res = true;
		}
		file.close();
	} else {
		// Handle the error: file could not be opened
		std::cerr << "Error: Unable to open file." << std::endl;
	}
	return res;
}

bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank, std::string& path) {
	setlocale(LC_ALL, ".UTF8");
	bool res = false;
	if (ImGui::Button("Load File"))
	{
		if (ImGuiFileDialog::Instance()->IsOpened())
			ImGuiFileDialog::Instance()->Close();
		ImGui::SetNextWindowSize(ImVec2(800, 400));
		IGFD::FileDialogConfig config;
		config.path = (path.empty() ? "." : path);
		ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
			".bin,.txt,.hgr,.dhr,.shr, #C10000", config);
	}
	
	// Display the file dialog
	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		// Check if a file was selected
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			path = ImGuiFileDialog::Instance()->GetCurrentPath();
			if (filePath.length() >= 4) {
				std::string extension = ImGuiFileDialog::Instance()->GetCurrentFilter();
				if (extension == ".hgr")
					res = MemoryLoadHGR(filePath);
				else if (extension == ".dhr")
					res =  MemoryLoadDHR(filePath);
				else if (extension == ".shr")
					res = MemoryLoadSHR(filePath);
				else if (extension == "#C10000")
					res = MemoryLoadSHR(filePath);
				if (res)
				{
					ImGuiFileDialog::Instance()->Close();
					return res;
				}
			}
			// If a file is selected, read and load it into the array
			if (filePath[0] != '\0') {
				res = MemoryLoad(filePath, position, bAuxBank);
				
				// Reset filePath for next operation
				filePath[0] = '\0';
			}
		}
		ImGuiFileDialog::Instance()->Close();

	}
	return res;
}

bool MemoryLoadHGR(const std::string &filePath) {
	bool res = false;
	uint8_t* pMem;
	pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
	std::ifstream file(filePath, std::ios::binary);
	if (file)
	{
		// Move to the end to get the file size
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		
		if (fileSize == 0x2000) {
			file.seekg(0, std::ios::beg); // Go back to the start of the file
			file.read(reinterpret_cast<char*>(pMem), fileSize);
			res = true;
		} else {
			std::cerr << "Error: HGR file is not the correct size." << std::endl;
		}
	}
	return res;
}

bool MemoryLoadDHR(const std::string &filePath) {
	bool res = false;
	uint8_t* pMem;
	std::ifstream file(filePath, std::ios::binary);
	if (file)
	{
		// Move to the end to get the file size
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		
		if (fileSize == 0x4000) {
			file.seekg(0, std::ios::beg); // Go back to the start of the file
			// Read first the aux and then the main memory
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
			file.read(reinterpret_cast<char*>(pMem), 0x2000);
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
			file.read(reinterpret_cast<char*>(pMem), 0x2000);
			res = true;
		} else {
			std::cerr << "Error: DHR file is not the correct size." << std::endl;
		}
	}
	return res;
}

bool MemoryLoadSHR(const std::string &filePath) {
	bool res = false;
	uint8_t* pMem;
	pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
	std::ifstream file(filePath, std::ios::binary);
	if (file)
	{
		// Move to the end to get the file size
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		
		if (fileSize >= 0x8000) {
			file.seekg(0, std::ios::beg); // Go back to the start of the file
			file.read(reinterpret_cast<char*>(pMem), 0x8000);
			res = true;
			if (fileSize > 0x8000) {
				// load the rest as "interlaced" SHR in main memory
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
				file.read(reinterpret_cast<char*>(pMem), fileSize < (0x8000 * 2) ? fileSize - 0x8000 : 0x8000);
			}
		}
	}
	return res;
}
