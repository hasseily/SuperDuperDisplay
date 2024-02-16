#include "EventRecorder.h"
#include "common.h"
#include "SDHRManager.h"
#include "imgui.h"
#include "imgui_internal.h"		// for PushItemFlag
#include "extras/ImGuiFileDialog.h"
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

constexpr uint32_t MAXRECORDING_SECONDS = 30;	// Max number of seconds to record

// below because "The declaration of a static data member in its class definition is not a definition"
EventRecorder* EventRecorder::s_instance;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::Initialize()
{
	ClearRecording();
}

EventRecorder::~EventRecorder()
{
	delete[] memStartState;
}

//////////////////////////////////////////////////////////////////////////
// Serialization and data transfer methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::GetRAMSnapshot()
{
	memset(memStartState, 0, sizeof(memStartState));
	auto _memsize = _A2_MEMORY_SHADOW_END - _A2_MEMORY_SHADOW_BEGIN;
	// Get the MAIN chunk
	memcpy_s(memStartState + _A2_MEMORY_SHADOW_BEGIN, _memsize,
		SDHRManager::GetInstance()->GetApple2MemPtr(), _memsize);
	// Get the AUX chunk
	memcpy_s(memStartState + 0x10000 + _A2_MEMORY_SHADOW_BEGIN, _memsize,
		SDHRManager::GetInstance()->GetApple2MemAuxPtr(), _memsize);
}

void EventRecorder::ApplyRAMSnapshot()
{
	auto _memsize = _A2_MEMORY_SHADOW_END - _A2_MEMORY_SHADOW_BEGIN;
	// Get the MAIN chunk
	memcpy_s(SDHRManager::GetInstance()->GetApple2MemPtr(), _memsize,
		memStartState + _A2_MEMORY_SHADOW_BEGIN, _memsize);
	// Get the AUX chunk
	memcpy_s(SDHRManager::GetInstance()->GetApple2MemAuxPtr(), _memsize,
		memStartState + 0x10000 + _A2_MEMORY_SHADOW_BEGIN, _memsize);
}

void EventRecorder::WriteEvent(const SDHREvent& event, std::ofstream& file) {
	// Serialize and write each member of SDHREvent to the file
	file.write(reinterpret_cast<const char*>(&event.is_iigs), sizeof(event.is_iigs));
	file.write(reinterpret_cast<const char*>(&event.m2b0), sizeof(event.m2b0));
	file.write(reinterpret_cast<const char*>(&event.rw), sizeof(event.rw));
	file.write(reinterpret_cast<const char*>(&event.addr), sizeof(event.addr));
	file.write(reinterpret_cast<const char*>(&event.data), sizeof(event.data));
}

void EventRecorder::ReadEvent(std::ifstream& file) {
	auto event = SDHREvent(false, false, 0, 0, 0);
	file.read(reinterpret_cast<char*>(&event.is_iigs), sizeof(event.is_iigs));
	file.read(reinterpret_cast<char*>(&event.m2b0), sizeof(event.m2b0));
	file.read(reinterpret_cast<char*>(&event.rw), sizeof(event.rw));
	file.read(reinterpret_cast<char*>(&event.addr), sizeof(event.addr));
	file.read(reinterpret_cast<char*>(&event.data), sizeof(event.data));
	v_events.push_back(event);
}

void EventRecorder::StopReplay()
{
	if (bIsInReplayMode)
	{
		bShouldStopReplay = true;
		if (thread_replay.joinable())
			thread_replay.join();
	}
	// Don't automatically exit replay mode
	// Let the user do it manually
	// bIsInReplayMode = false;
}

void EventRecorder::StartReplay()
{
	RewindReplay();
	bShouldPauseReplay = false;
	bShouldStopReplay = false;
	bIsInReplayMode = true;
	thread_replay = std::thread(&EventRecorder::replay_events_thread, this,
		&bShouldPauseReplay, &bShouldStopReplay, &currentReplayEvent);
}

void EventRecorder::PauseReplay(bool pause)
{
	bShouldPauseReplay = pause;
}

void EventRecorder::RewindReplay()
{
	StopReplay();
	ApplyRAMSnapshot();
	currentReplayEvent = 0;
}


int EventRecorder::replay_events_thread(bool* shouldPauseReplay, bool* shouldStopReplay, size_t* currentReplayEvent)
{
	using namespace std::chrono;
	auto targetDuration = duration<double, std::nano>(977.7778337);	// Duration of an Apple 2 clock cycle (not stretched)
	auto startTime = high_resolution_clock::now();
	auto elapsed = high_resolution_clock::now() - startTime;

	while (!*shouldStopReplay)
	{
		if (*shouldPauseReplay)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		if (*currentReplayEvent < v_events.size())
		{
			insert_event(&v_events.at(*currentReplayEvent));
			*currentReplayEvent += 1;
			// wait 1 clock cycle before adding the next event
			startTime = high_resolution_clock::now();
			while (true)
			{
				elapsed = high_resolution_clock::now() - startTime;
				if (elapsed >= targetDuration)
					break;
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Main methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::StartRecording()
{
	ClearRecording();
	v_events.reserve(1000000 * MAXRECORDING_SECONDS);
	GetRAMSnapshot();
	bIsRecording = true;
}

void EventRecorder::StopRecording()
{
	bIsRecording = false;
	bHasRecording = true;
	SaveRecording();
}

void EventRecorder::ClearRecording()
{
	memset(memStartState, 0, sizeof(memStartState));
	v_events.clear();
	v_events.shrink_to_fit();
	bHasRecording = false;
	currentReplayEvent = 0;
}

void EventRecorder::SaveRecording()
{
	IGFD::FileDialogConfig config;
	config.path = "./recordings/";
	ImGui::SetNextWindowSize(ImVec2(800, 400));
	ImGuiFileDialog::Instance()->OpenDialog("ChooseRecordingSave", "Save to File", ".vcr,", config);
}

void EventRecorder::LoadRecording()
{
	IGFD::FileDialogConfig config;
	config.path = "./recordings/";
	ImGui::SetNextWindowSize(ImVec2(800, 400));
	ImGuiFileDialog::Instance()->OpenDialog("ChooseRecordingLoad", "Load Recording File", ".vcr,", config);
}



//////////////////////////////////////////////////////////////////////////
// Public methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::RecordEvent(SDHREvent* sdhr_event)
{
	if (!bIsRecording)
		return;
	v_events.push_back(*sdhr_event);
	++currentReplayEvent;
	if (v_events.size() == 1000000 * MAXRECORDING_SECONDS)
		StopRecording();
}

void EventRecorder::DisplayImGuiRecorderWindow(bool* p_open)
{
	if (p_open)
	{
		ImGui::Begin("Event Recorder", p_open);
		ImGui::PushItemWidth(200);

		if (bIsRecording)
			ImGui::Text("RECORDING IN PROGRESS...");
		else {
			if (bHasRecording)
				ImGui::Text("Recording available");
			else
				ImGui::Text("No recording loaded");
		}

		if (bIsRecording == true)
		{
			if (ImGui::Button("Stop Recording"))
				this->StopRecording();
		}
		else {
			if (ImGui::Button("Start Recording"))
				this->StartRecording();
		}

		ImGui::Checkbox("Replay Mode", &bIsInReplayMode);
		ImGui::SliderInt("Event Timeline", reinterpret_cast<int*>(&currentReplayEvent), 0, v_events.size());
		if (thread_replay.joinable())
		{
			if (ImGui::Button("Stop##Replay"))
				this->StopReplay();
		}
		else {
			if (ImGui::Button("Play##Replay"))
				this->StartReplay();
		}
		ImGui::SameLine();
		if (bShouldPauseReplay)
		{
			if (ImGui::Button("Unpause##Recording"))
				this->PauseReplay(false);
		}
		else {
			if (ImGui::Button("Pause##Recording"))
				this->PauseReplay(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Rewind##Recording"))
			this->RewindReplay();
		ImGui::Separator();

		if (ImGui::Button("Load##Recording"))
		{
			this->LoadRecording();
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(50.0f, 0.0f));
		ImGui::SameLine();
		if (bHasRecording == false)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		if (ImGui::Button("Save##Recording"))
		{
			this->SaveRecording();
		}
		if (bHasRecording == false)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		ImGui::PopItemWidth();

		// Display the load file dialog
		if (ImGuiFileDialog::Instance()->Display("ChooseRecordingLoad")) {
			// Check if a file was selected
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::ifstream file(ImGuiFileDialog::Instance()->GetFilePathName().c_str(), std::ios::binary);
				if (file.is_open()) {
					ClearRecording();
					v_events.reserve(1000000 * MAXRECORDING_SECONDS);
					try
					{
						// First load the initial RAM state
						file.read(reinterpret_cast<char*>(memStartState), (RECORDER_TOTALMEMSIZE) / sizeof(uint8_t));
						// Next read the event vector size
						size_t _size;
						file.read(reinterpret_cast<char*>(&_size), sizeof(_size));
						std::cout << "Reading " << _size << " events from file" << std::endl;
						// And finally the events
						if (_size > 0)
						{
							for (size_t i = 0; i < _size; ++i) {
								ReadEvent(file);
							}
						}
					}
					catch (std::ifstream::failure& e)
					{
						m_lastErrorString = e.what();
						bImGuiOpenModal = true;
						ImGui::OpenPopup("Recorder Error Modal");
					}
					file.close();
					bHasRecording = true;
				}
				else {
					m_lastErrorString = "Error opening file";
					bImGuiOpenModal = true;
					ImGui::OpenPopup("Recorder Error Modal");
				}
			}
			ImGuiFileDialog::Instance()->Close();
		}

		// Display the save file dialog
		if (ImGuiFileDialog::Instance()->Display("ChooseRecordingSave")) {
			// Check if a file was selected
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::ofstream file(ImGuiFileDialog::Instance()->GetFilePathName().c_str(), std::ios::binary);
				if (file.is_open()) {
					try
					{
						// First store the initial RAM state
						file.write(reinterpret_cast<const char*>(memStartState), (RECORDER_TOTALMEMSIZE) / sizeof(uint8_t));
						// Next store the event vector size
						auto _size = v_events.size();
						file.write(reinterpret_cast<const char*>(&_size), sizeof(_size));
						std::cout << "Writing " << _size << " events to file" << std::endl;
						// And finally the events
						for (const auto& event : v_events) {
							WriteEvent(event, file);
						}
						file.close();
					}
					catch (std::ofstream::failure& e)
					{
						m_lastErrorString = e.what();
						bImGuiOpenModal = true;
						ImGui::OpenPopup("Recorder Error Modal");
					}
				}
				else {
					m_lastErrorString = "Error opening file";
					bImGuiOpenModal = true;
					ImGui::OpenPopup("Recorder Error Modal");
				}
			}
			ImGuiFileDialog::Instance()->Close();
		}

		if (bImGuiOpenModal) {
			if (ImGui::BeginPopupModal("Recorder Error Modal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text(m_lastErrorString.c_str());
				// Buttons to close the modal
				if (ImGui::Button("OK", ImVec2(120, 0))) {
					// Handle OK (e.g., process data, close modal)
					ImGui::CloseCurrentPopup();
					bImGuiOpenModal = false;
				}
				ImGui::EndPopup();
			}
		}

		ImGui::End();
	}
}

void EventRecorder::Update()
{
	// Stop replay if we reached the end
	if (bIsInReplayMode)
	{
		if (currentReplayEvent >= v_events.size())
			PauseReplay(true);
	}
}