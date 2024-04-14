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

bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank) {
	bool res = false;
	uint8_t* pMem;
	if (bAuxBank)
		pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + position;
	else
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + position;
	if (ImGui::Button("Load File in Memory"))
	{
		ImGui::SetNextWindowSize(ImVec2(800, 400));
		IGFD::FileDialogConfig config;
		config.path = ".";
		ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".bin,.txt,.hgr,.dhr,.shr", config);
	}
	
	// Display the file dialog
	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		// Check if a file was selected
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			ImGuiFileDialog::Instance()->Close();
			if (filePath.length() >= 4) {
				// Extract the last 4 characters
				std::string extension = filePath.substr(filePath.length() - 4, 4);
				if (extension == ".hgr")
					return MemoryLoadHGR(filePath);
				if (extension == ".dhr")
					return MemoryLoadDHR(filePath);
				if (extension == ".shr")
					return MemoryLoadSHR(filePath);
			}
			// If a file is selected, read and load it into the array
			if (filePath[0] != '\0') {
				std::ifstream file(filePath, std::ios::binary);
				if (file) {
					// Move to the end to get the file size
					file.seekg(0, std::ios::end);
					size_t fileSize = file.tellg();
					
					if (fileSize > 0 && (position + fileSize) <= (_A2_MEMORY_SHADOW_END)) {
						file.seekg(0, std::ios::beg); // Go back to the start of the file
						file.read(reinterpret_cast<char*>(pMem), fileSize);
						res = true;
					} else {
						// Handle the error: file is too big or other issues
						std::cerr << "Error: File is too large or other issue." << std::endl;
					}
					file.close();
				} else {
					// Handle the error: file could not be opened
					std::cerr << "Error: Unable to open file." << std::endl;
				}
				
				// Reset filePath for next operation
				filePath[0] = '\0';
			}
		}
	}
	return res;
}

bool MemoryLoadHGR(std::string filePath) {
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

bool MemoryLoadDHR(std::string filePath) {
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

bool MemoryLoadSHR(std::string filePath) {
	bool res = false;
	uint8_t* pMem;
	pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
	std::ifstream file(filePath, std::ios::binary);
	if (file)
	{
		// Move to the end to get the file size
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		
		if (fileSize == 0x8000) {
			file.seekg(0, std::ios::beg); // Go back to the start of the file
			file.read(reinterpret_cast<char*>(pMem), fileSize);
			res = true;
		} else {
			std::cerr << "Error: SHR file is not the correct size." << std::endl;
		}
	}
	return res;
}
