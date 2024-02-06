// Extras class to load a binary file into an arbitrary location in memory

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include "../SDHRManager.h"
#include "ImGuiFileDialog.h"

static void MemoryLoad(uint32_t position, bool bAuxBank) {
	uint8_t* pMem;
	if (bAuxBank)
		pMem = SDHRManager::GetInstance()->GetApple2MemAuxPtr() + position;
	else
		pMem = SDHRManager::GetInstance()->GetApple2MemPtr() + position;
	if (ImGui::Button("Load File in Memory"))
	{
		ImGui::SetNextWindowSize(ImVec2(800, 400));
		IGFD::FileDialogConfig config;
		config.path = ".";
		ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".bin,.txt,", config);
	}

	// Display the file dialog
	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		// Check if a file was selected
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
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
		ImGuiFileDialog::Instance()->Close();
	}
}

#endif /* MEMORYLOADER_H */
