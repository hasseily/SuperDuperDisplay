#include "EventRecorder.h"
#include "common.h"
#include "MemoryManager.h"
#include "CycleCounter.h"
#include "A2VideoManager.h"
#include "imgui.h"
#include "imgui_internal.h"		// for PushItemFlag
#include "extras/ImGuiFileDialog.h"
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>

constexpr uint32_t MAXRECORDING_SECONDS = 30;	// Max number of seconds to record

// below because "The declaration of a static data member in its class definition is not a definition"
EventRecorder* EventRecorder::s_instance;

// Mem snapshot cycles may be different in saved recordings
static size_t m_current_snapshot_cycles = RECORDER_MEM_SNAPSHOT_CYCLES;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::Initialize()
{
	ClearRecording();
}

EventRecorder::~EventRecorder()
{

}

//////////////////////////////////////////////////////////////////////////
// Serialization and data transfer methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::MakeRAMSnapshot(size_t cycle)
{
	(void)cycle; // mark as unused
	auto _memsize = _A2_MEMORY_SHADOW_END - _A2_MEMORY_SHADOW_BEGIN;
	ByteBuffer buffer(RECORDER_TOTALMEMSIZE);
	// Get the MAIN chunk
	buffer.copyFrom(MemoryManager::GetInstance()->GetApple2MemPtr(), _A2_MEMORY_SHADOW_BEGIN, _memsize);
	// Get the AUX chunk
	buffer.copyFrom(MemoryManager::GetInstance()->GetApple2MemAuxPtr(), 0x10000 + _A2_MEMORY_SHADOW_BEGIN, _memsize);

	v_memSnapshots.push_back(std::move(buffer));
}

void EventRecorder::ApplyRAMSnapshot(size_t snapshot_index)
{
	if (snapshot_index > (v_memSnapshots.size() - 1))
		std::cerr << "ERROR: Requested to apply nonexistent memory snapshot at index " << snapshot_index << std::endl;
	auto _memsize = _A2_MEMORY_SHADOW_END - _A2_MEMORY_SHADOW_BEGIN;
	// Set the MAIN chunk
	v_memSnapshots.at(snapshot_index).copyTo(MemoryManager::GetInstance()->GetApple2MemPtr(), _A2_MEMORY_SHADOW_BEGIN, _memsize);
	// Set the AUX chunk
	v_memSnapshots.at(snapshot_index).copyTo(MemoryManager::GetInstance()->GetApple2MemAuxPtr(), 0x10000 + _A2_MEMORY_SHADOW_BEGIN, _memsize);
}

void EventRecorder::WriteRecordingFile(std::ofstream& file)
{
	// First store the RAM snapshot interval
	file.write(reinterpret_cast<const char*>(&m_current_snapshot_cycles), sizeof(m_current_snapshot_cycles));
	// Next store the event vector size
	auto _size = v_events.size();
	file.write(reinterpret_cast<const char*>(&_size), sizeof(_size));
	// Next store the RAM states
	for (const auto& snapshot : v_memSnapshots) {
		file.write(reinterpret_cast<const char*>(snapshot.data()), (RECORDER_TOTALMEMSIZE) / sizeof(uint8_t));
	}

	std::cout << "Writing " << _size << " events to file" << std::endl;
	// And finally the events
	for (const auto& event : v_events) {
		WriteEvent(event, file);
	}
}

void EventRecorder::ReadRecordingFile(std::ifstream& file)
{
	ClearRecording();
	v_events.reserve(1000000 * MAXRECORDING_SECONDS);
	// First read the ram snapshot interval
	file.read(reinterpret_cast<char*>(&m_current_snapshot_cycles), sizeof(m_current_snapshot_cycles));
	// Next read the event vector size
	size_t _size;
	file.read(reinterpret_cast<char*>(&_size), sizeof(_size));
	// Then all the RAM states
	if (_size > 0)
	{
		for (size_t i = 0; i <= (_size / m_current_snapshot_cycles); ++i) {
			auto snapshot = ByteBuffer(RECORDER_TOTALMEMSIZE);
			file.read(reinterpret_cast<char*>(snapshot.data()), (RECORDER_TOTALMEMSIZE) / sizeof(uint8_t));
			v_memSnapshots.push_back(std::move(snapshot));
		}
	}
	std::cout << "Reading " << _size << " events from file" << std::endl;
	// And finally the events
	if (_size > 0)
	{
		for (size_t i = 0; i < _size; ++i) {
			ReadEvent(file);
		}
	}
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
	auto event = SDHREvent(false, false, false, 0, 0, 0);
	file.read(reinterpret_cast<char*>(&event.is_iigs), sizeof(event.is_iigs));
	file.read(reinterpret_cast<char*>(&event.m2b0), sizeof(event.m2b0));
	file.read(reinterpret_cast<char*>(&event.rw), sizeof(event.rw));
	file.read(reinterpret_cast<char*>(&event.addr), sizeof(event.addr));
	file.read(reinterpret_cast<char*>(&event.data), sizeof(event.data));
	v_events.push_back(std::move(event));
}

//////////////////////////////////////////////////////////////////////////
// Replay methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::StopReplay()
{
	if (m_state == EventRecorderStates_e::PLAYING)
	{
		bShouldStopReplay = true;
		if (thread_replay.joinable())
			thread_replay.join();
	}
}

void EventRecorder::StartReplay()
{
	if (!bHasRecording)
		return;
	RewindReplay();
	bShouldPauseReplay = false;
	bShouldStopReplay = false;
	SetState(EventRecorderStates_e::PLAYING);
	ApplyRAMSnapshot(0);
	thread_replay = std::thread(&EventRecorder::replay_events_thread, this,
		&bShouldPauseReplay, &bShouldStopReplay);
}

void EventRecorder::PauseReplay(bool pause)
{
	bShouldPauseReplay = pause;
}

void EventRecorder::RewindReplay()
{
	StopReplay();
	currentReplayEvent = 0;
}

int EventRecorder::replay_events_thread(bool* shouldPauseReplay, bool* shouldStopReplay)
{
	using namespace std::chrono;
	auto targetDuration = duration<double, std::nano>(1977.7778337);	// Duration of an Apple 2 clock cycle (not stretched)
	auto startTime = high_resolution_clock::now();
	auto elapsed = high_resolution_clock::now() - startTime;

	while (!*shouldStopReplay)
	{
		if (*shouldPauseReplay)
		{
			SetState(EventRecorderStates_e::PAUSED);
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		SetState(EventRecorderStates_e::PLAYING);
		// Check if the user requested to move to a different area in the recording
		if (bUserMovedEventSlider)
		{
			// Move to the requested event. In order to do this cleanly, we need:
			// 1. to find the closest previous memory snapshot
			// 2. run all events between the mem snapshot and the requested event
			// These events can be run at max speed
			auto snapshot_index = currentReplayEvent / m_current_snapshot_cycles;
			ApplyRAMSnapshot(snapshot_index);
			bool isVBL = false;
			auto first_event_index = snapshot_index * m_current_snapshot_cycles;
			for (auto i = first_event_index; i < currentReplayEvent; i++)
			{
				auto e = v_events.at(i);
				isVBL = ((e.addr == 0xC019) && e.rw && ((e.data >> 7) == (e.is_iigs ? 1 : 0)));
				CycleCounter::GetInstance()->IncrementCycles(1, isVBL);
				process_single_event(e);
			}
		}

		if (currentReplayEvent < v_events.size())
		{
			if (*shouldStopReplay)	// In case a stop was sent while sleeping
				break;
			auto e = v_events.at(currentReplayEvent);
			bool isVBL = ((e.addr == 0xC019) && e.rw && ((e.data >> 7) == (e.is_iigs ? 1 : 0)));
			CycleCounter::GetInstance()->IncrementCycles(1, isVBL);
			process_single_event(e);
			currentReplayEvent += 1;
			// wait 1 clock cycle before adding the next event
			startTime = high_resolution_clock::now();
			while (true)
			{
				elapsed = high_resolution_clock::now() - startTime;
				if (elapsed >= targetDuration)
					break;
			}
		}
	}
	SetState(EventRecorderStates_e::STOPPED);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Recording methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::StartRecording()
{
	ClearRecording();
	v_events.reserve(1000000 * MAXRECORDING_SECONDS);
	SetState(EventRecorderStates_e::RECORDING);
}

void EventRecorder::StopRecording()
{
	bHasRecording = true;
	SaveRecording();
	SetState(EventRecorderStates_e::STOPPED);
}

void EventRecorder::ClearRecording()
{
	v_memSnapshots.clear();
	v_events.clear();
	v_events.shrink_to_fit();
	bHasRecording = false;
	currentReplayEvent = 0;
	m_current_snapshot_cycles = RECORDER_MEM_SNAPSHOT_CYCLES;
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
	SetState(EventRecorderStates_e::STOPPED);
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
	if (m_state != EventRecorderStates_e::RECORDING)
		return;
	if ((currentReplayEvent % RECORDER_MEM_SNAPSHOT_CYCLES) == 0)
		MakeRAMSnapshot(currentReplayEvent);
	v_events.push_back(*sdhr_event);
	++currentReplayEvent;
	if (v_events.size() == (size_t)1'000'000 * MAXRECORDING_SECONDS)
		StopRecording();
}

void EventRecorder::DisplayImGuiWindow(bool* p_open)
{
	if (p_open)
	{
		ImGui::Begin("Event Recorder", p_open);
		ImGui::PushItemWidth(200);

		if (m_state == EventRecorderStates_e::RECORDING)
			ImGui::Text("RECORDING IN PROGRESS...");
		else {
			if (bHasRecording)
				ImGui::Text("Recording available");
			else
				ImGui::Text("No recording loaded");
		}

		if (m_state == EventRecorderStates_e::RECORDING)
		{
			if (ImGui::Button("Stop Recording"))
				this->StopRecording();
		}
		else {
			if (ImGui::Button("Start Recording"))
				this->StartRecording();
		}

		static bool bIsInReplayMode = (this->IsInReplayMode());
		if (ImGui::Checkbox("Replay Mode", &bIsInReplayMode))
		{
			if (bIsInReplayMode)
				SetState(EventRecorderStates_e::STOPPED);
			else
				SetState(EventRecorderStates_e::DISABLED);
		}
		bUserMovedEventSlider = ImGui::SliderInt("Event Timeline", reinterpret_cast<int*>(&currentReplayEvent), 0, (int)v_events.size());
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
		if (m_state == EventRecorderStates_e::PAUSED)
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
					try
					{
						ReadRecordingFile(file);

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
						WriteRecordingFile(file);
					}
					catch (std::ofstream::failure& e)
					{
						m_lastErrorString = e.what();
						bImGuiOpenModal = true;
						ImGui::OpenPopup("Recorder Error Modal");
					}
					file.close();
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
				ImGui::Text("%s", m_lastErrorString.c_str());
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

void EventRecorder::SetState(EventRecorderStates_e _state)
{
	if (_state == m_state)
		return;
	m_state = _state;
	switch (m_state) {
		case EventRecorderStates_e::DISABLED:
			A2VideoManager::GetInstance()->ActivateBeam();
			break;
		case EventRecorderStates_e::STOPPED:
			A2VideoManager::GetInstance()->DeactivateBeam();
			break;
		case EventRecorderStates_e::PAUSED:
			A2VideoManager::GetInstance()->DeactivateBeam();
			break;
		case EventRecorderStates_e::PLAYING:
			A2VideoManager::GetInstance()->ActivateBeam();
			break;
		case EventRecorderStates_e::RECORDING:
			A2VideoManager::GetInstance()->ActivateBeam();
			break;
		case EventRecorderStates_e::TOTAL_COUNT:
			break;
	}
}
