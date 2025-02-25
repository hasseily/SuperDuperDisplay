#include "EventRecorder.h"
#include "common.h"
#include "MemoryManager.h"
#include "CycleCounter.h"
#include "A2VideoManager.h"
#include "MockingboardManager.h"
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
	slowdownMultiplier = 1;
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
	auto _memsize = _A2_MEMORY_SHADOW_END;
	ByteBuffer buffer(RECORDER_TOTALMEMSIZE);
	// Get the MAIN chunk
	buffer.copyFrom(MemoryManager::GetInstance()->GetApple2MemPtr(), 0, _memsize);
	// Get the AUX chunk
	buffer.copyFrom(MemoryManager::GetInstance()->GetApple2MemAuxPtr(), 0x10000, _memsize);

	v_memSnapshots.push_back(std::move(buffer));
}

void EventRecorder::ApplyRAMSnapshot(size_t snapshot_index)
{
	if (snapshot_index > (v_memSnapshots.size() - 1))
		std::cerr << "ERROR: Requested to apply nonexistent memory snapshot at index " << snapshot_index << std::endl;
	auto _memsize = _A2_MEMORY_SHADOW_END;
	// Set the MAIN chunk
	v_memSnapshots.at(snapshot_index).copyTo(MemoryManager::GetInstance()->GetApple2MemPtr(), 0, _memsize);
	// Set the AUX chunk
	v_memSnapshots.at(snapshot_index).copyTo(MemoryManager::GetInstance()->GetApple2MemAuxPtr(), 0x10000, _memsize);
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
	StopReplay();
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
	bHasRecording = true;
}

void EventRecorder::ReadTextEventsFromFile(std::ifstream& file)
{
	StopReplay();
	ClearRecording();
	MakeRAMSnapshot(0);	// Just make a snapshot of what is now
	v_events.reserve(1000000 * MAXRECORDING_SECONDS);
	
	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') {
			continue; // Skip empty lines or lines starting with '#'
		}
		
		std::stringstream ss(line);
		std::string count_str, is_iigs_str, m2b0_str, m2sel_str, rw_str, addr_str, data_str;
		
		std::getline(ss, count_str, ',');
		std::getline(ss, is_iigs_str, ',');
		std::getline(ss, m2b0_str, ',');
		std::getline(ss, m2sel_str, ',');
		std::getline(ss, rw_str, ',');
		std::getline(ss, addr_str, ',');
		std::getline(ss, data_str, ',');
		
		auto count = std::stoul(count_str);
		bool is_iigs = std::stoi(is_iigs_str);
		bool m2b0 = std::stoi(m2b0_str);
		bool m2sel = std::stoi(m2sel_str);
		bool rw = std::stoi(rw_str);
		uint16_t addr = std::stoul(addr_str, nullptr, 16);
		uint8_t data = std::stoul(data_str, nullptr, 16);
		
		SDHREvent event(is_iigs, m2b0, m2sel, rw, addr, data);
		for (size_t i = 0; i < count; ++i) {
			v_events.push_back(event);
		}
	}

	std::cout << "Read " << v_events.size() << " text events from file" << std::endl;
	bHasRecording = true;
}

// Reading an animation file locally
// The PaintWorks animations format is:
// 0x0000-0x7FFF: first SHR animation frame, standard SHR
// 0x8000-0x8003: length of animation data block (starting at 0x8008)
// 0x8004-0x8007: delay time per frame, in 60th of a second
// 0x8008-0x8011: disregard (sometimes offset to starting records)
// 0x8011-EOF   : animations data block: 2 byte offset, 2 byte value
// If offset is zero, it's the end of the frame
// Offset is to the start of the SHR image, so need to add 0x2000 in AUX mem
void EventRecorder::ReadPaintWorksAnimationsFile(std::ifstream& file)
{
	StopReplay();
	ClearRecording();
	v_events.reserve(1000000 * MAXRECORDING_SECONDS);
	auto pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
	// Read first SHR frame
	file.read(reinterpret_cast<char*>(pMem), 0x8000);
	// Then the animations block length
	uint32_t dbaLength = 0;
	file.read(reinterpret_cast<char*>(&dbaLength), 4);
	// Then the frame delay
	uint32_t frameDelay = 0;
	file.read(reinterpret_cast<char*>(&frameDelay), 4);
	// And the unused offset
	uint32_t _unusedOffset = 0;
	file.read(reinterpret_cast<char*>(&_unusedOffset), 4);
	MakeRAMSnapshot(0);	// Make a snapshot now, before doing the animations events
	if (dbaLength > 4)
	{
		dbaLength -= 4;
		uint16_t _off = 0;
		uint8_t _valHi = 0;
		uint8_t _valLo = 0;
		
		v_events.push_back(SDHREvent(false, false, false, false, 0xC005, 0));	// RAMWRTON
		for (uint32_t i = 0; i < (dbaLength / 4); ++i)
		{
			file.read(reinterpret_cast<char*>(&_off), 2);
			file.read(reinterpret_cast<char*>(&_valHi), 1);
			file.read(reinterpret_cast<char*>(&_valLo), 1);
			if (_off != 0)
			{
				v_events.push_back(SDHREvent(false, false, false, false, _off + 0x2000, _valHi));
				v_events.push_back(SDHREvent(false, false, false, false, _off + 0x2001, _valLo));
			} else {
				// add the delay between the frames using a dummy read event
				for (size_t _d = 0; _d < ((size_t)frameDelay * (1'000'000 / 60)); ++_d) {
					v_events.push_back(SDHREvent(false, false, false, true, 0, 0));
				}
			}
		}
		v_events.push_back(SDHREvent(false, false, false, false, 0xC004, 0));	// RAMWRTOFF
	}
	bHasRecording = true;
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
	bUserMovedEventSlider = true;
}

int EventRecorder::replay_events_thread(bool* shouldPauseReplay, bool* shouldStopReplay)
{
	using namespace std::chrono;
	// Duration of an Apple 2 clock cycle (not stretched)
	auto duration_ns = 1'000'000'000 / (bIsPAL ? _A2_CPU_FREQUENCY_PAL : _A2_CPU_FREQUENCY_NTSC);
	auto targetDuration = duration_cast<high_resolution_clock::duration>(duration<double, std::nano>(slowdownMultiplier * duration_ns));
	auto startTime = high_resolution_clock::now();
	auto nextTime = startTime;

	while (!*shouldStopReplay)
	{
		if (*shouldPauseReplay)
		{
			SetState(EventRecorderStates_e::PAUSED);
			std::this_thread::sleep_for(seconds(1));
			continue;
		}
		if (GetState() != EventRecorderStates_e::PLAYING)
		{
			SetState(EventRecorderStates_e::PLAYING);
			startTime = high_resolution_clock::now();
			nextTime = startTime;
		}
		// Check if the user requested to move to a different area in the recording
		if (bUserMovedEventSlider)
		{
			bUserMovedEventSlider = false;
			// Move to the requested event. In order to do this cleanly, we need:
			// 1. to find the closest previous memory snapshot
			// 2. run all events between the mem snapshot and the requested event
			// These events can be run at max speed
			auto snapshot_index = currentReplayEvent / m_current_snapshot_cycles;
			ApplyRAMSnapshot(snapshot_index);
			auto first_event_index = snapshot_index * m_current_snapshot_cycles;
			for (auto i = first_event_index; i < currentReplayEvent; i++)
			{
				auto e = v_events.at(i);
				process_single_event(e);
			}
		}

		if ((v_events.size() > 0) && (currentReplayEvent < v_events.size()))
		{
			if (*shouldStopReplay)	// In case a stop was sent while sleeping
				break;
			auto e = v_events.at(currentReplayEvent);
			process_single_event(e);
			currentReplayEvent += 1;
			// wait 1 clock cycle before adding the next event, compensating for drift
			nextTime += targetDuration;
			while (true)
			{
				if (high_resolution_clock::now() >= nextTime)
					break;
			}
			startTime = nextTime;
		}
		else {
			currentReplayEvent = 0;
			ApplyRAMSnapshot(0);
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
	ImGuiFileDialog::Instance()->OpenDialog("ChooseRecordingLoad", "Load Recording File", ".vcr,.shra,#C20000", config);
}

void EventRecorder::LoadTextEventsFromFile()
{
	SetState(EventRecorderStates_e::STOPPED);
	IGFD::FileDialogConfig config;
	config.path = "./recordings/";
	ImGui::SetNextWindowSize(ImVec2(800, 400));
	ImGuiFileDialog::Instance()->OpenDialog("ChooseTextEventsFileLoad", "Load Text Events File", ".csv,", config);
}

//////////////////////////////////////////////////////////////////////////
// Public methods
//////////////////////////////////////////////////////////////////////////

void EventRecorder::SetPAL(bool isPal) {
	// Don't stop playing here because this may be called from the playing thread
	bIsPAL = isPal;
}

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
		if (bHasRecording == false)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		bUserMovedEventSlider = ImGui::SliderInt("Event Timeline", reinterpret_cast<int*>(&currentReplayEvent), 0, (int)v_events.size());
		if (bIsInReplayMode)
		{
			if (ImGui::InputInt("X Slowdown", &slowdownMultiplier))
			{
				if (slowdownMultiplier < 0)
					slowdownMultiplier = 0;
				// make sure targetDuration is updated
				this->PauseReplay(true);
				this->PauseReplay(false);
			}
		}
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
		if (bHasRecording == false)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		ImGui::Separator();

		if (ImGui::Button("Load##Recording"))
		{
			this->LoadRecording();
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(50.0f, 0.0f));
		ImGui::SameLine();
		if (ImGui::Button("Load CSV##Recording"))
		{
			this->LoadTextEventsFromFile();
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
				auto _fileExtension = ImGuiFileDialog::Instance()->GetCurrentFilter();
				std::ifstream file(ImGuiFileDialog::Instance()->GetFilePathName().c_str(), std::ios::binary);
				if (file.is_open()) {
					try
					{
						if (_fileExtension == ".vcr")
							ReadRecordingFile(file);
						else if (_fileExtension == ".shra")
							ReadPaintWorksAnimationsFile(file);
						else if (_fileExtension == "#C20000")
							ReadPaintWorksAnimationsFile(file);
					}
					catch (std::ifstream::failure& e)
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
		
		// Display the load csv file dialog
		if (ImGuiFileDialog::Instance()->Display("ChooseTextEventsFileLoad")) {
			// Check if a file was selected
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::ifstream file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
				if (file.is_open()) {
					try
					{
						ReadTextEventsFromFile(file);
						
					}
					catch (std::ifstream::failure& e)
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
	/*
	// do things based on state
	switch (m_state) {
	default:
			break;
	}
	*/
}
