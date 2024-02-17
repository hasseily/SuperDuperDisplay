#pragma once
#ifndef EVENTRECORDER_H
#define EVENTRECORDER_H

/*
	Singleton event recorder class whose job is to:
		- keep the last 30M events in a buffer
		- store the state of RAM before the recording
		- stop if the recording reaches max
		- provide an ImGui interface to:
			- turn on-off recording
			- save and load recordings
			- replay recordings

*/

#include "common.h"
#include "SDHRNetworking.h"	// for SDHREvent
#include <vector>
#include <string>

#define RECORDER_TOTALMEMSIZE 128 * 1024		// 128k memory snapshot
#define RECORDER_MEM_SNAPSHOT_CYCLES 1'000'000	// snapshot memory every x cycles

class EventRecorder
{
public:
	void RecordEvent(SDHREvent* sdhr_event);
	void DisplayImGuiRecorderWindow(bool* p_open);
	void Update();	// call this from the main loop

	const bool IsRecording() {
		return bIsRecording;
	};
	const bool IsInReplayMode() {
		return bIsInReplayMode;
	};

	// public singleton code
	static EventRecorder* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new EventRecorder();
		return s_instance;
	}
	~EventRecorder();
private:
	void Initialize();

	// recording
	void StartRecording();
	void StopRecording();
	void ClearRecording();
	void SaveRecording();
	void LoadRecording();
	
	// replay
	void StopReplay();
	void StartReplay();
	void PauseReplay(bool pause);
	void RewindReplay();

	// de/serialization
	void MakeRAMSnapshot(size_t cycle);
	void ApplyRAMSnapshot(size_t snapshot_index);
	void WriteRecordingFile(std::ofstream& file);
	void ReadRecordingFile(std::ifstream& file);
	void WriteEvent(const SDHREvent& event, std::ofstream& file);
	void ReadEvent(std::ifstream& file);

	bool bIsRecording = false;
	bool bHasRecording = false;
	bool bIsInReplayMode = false;	// if in replay mode, don't process real events

	std::vector<ByteBuffer> v_memSnapshots;	// memory snapshots at regular intervals
	std::vector<SDHREvent> v_events;


	// Replay thread control
	std::thread thread_replay;
	int replay_events_thread(bool* shouldPauseReplay, bool* shouldStopReplay, size_t* currentReplayEvent);
	bool bShouldPauseReplay = false;
	bool bShouldStopReplay = false;
	size_t currentReplayEvent;		// index of the event ready to replay in the vector
	bool bUserMovedEventSlider;		// user moved the slider for events

	bool bImGuiOpenModal = false;
	std::string m_lastErrorString;

	//////////////////////////////////////////////////////////////////////////
	// Singleton pattern
	//////////////////////////////////////////////////////////////////////////
	static EventRecorder* s_instance;
	EventRecorder()
	{
		Initialize();
	}
};

#endif	// EVENTRECORDER_H
