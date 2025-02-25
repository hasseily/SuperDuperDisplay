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
#include <thread>

#define RECORDER_TOTALMEMSIZE 128 * 1024		// 128k memory snapshot
#define RECORDER_MEM_SNAPSHOT_CYCLES 1'000'000	// snapshot memory every x cycles

enum class EventRecorderStates_e
{
	DISABLED = 0,
	RECORDING,
	STOPPED,		// Anything at this state or later means the recorder is in REPLAY mode
	PAUSED,
	PLAYING,
	TOTAL_COUNT
};

class EventRecorder
{
public:
	void RecordEvent(SDHREvent* sdhr_event);
	void DisplayImGuiWindow(bool* p_open);
	void SetPAL(bool isPal);				// Sets PAL (true) or NTSC (false)
	inline const EventRecorderStates_e GetState() { return m_state; };
	inline const bool IsRecording() { return (m_state == EventRecorderStates_e::RECORDING); };
	inline const bool IsInReplayMode() { return (m_state >= EventRecorderStates_e::STOPPED); };

	// public singleton code
	static EventRecorder* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new EventRecorder();
		return s_instance;
	}
	~EventRecorder();

	// This method reads a binary recording file previously saved using SaveRecording()
	void ReadRecordingFile(std::ifstream& file);
	// This method reads a text event file, generally used for debugging
	void ReadTextEventsFromFile(std::ifstream& file);
	// This method reads a PaintWorks Animations file, also for debugging
	void ReadPaintWorksAnimationsFile(std::ifstream& file);
	void StopReplay();
	void StartReplay();

private:
	void Initialize();

	// recording
	void StartRecording();
	void StopRecording();
	void ClearRecording();
	void SaveRecording();
	void LoadRecording();
	void LoadTextEventsFromFile();
	
	// replay
	void PauseReplay(bool pause);
	void RewindReplay();

	// de/serialization
	void MakeRAMSnapshot(size_t cycle);
	void ApplyRAMSnapshot(size_t snapshot_index);
	void WriteRecordingFile(std::ofstream& file);
	void WriteEvent(const SDHREvent& event, std::ofstream& file);
	void ReadEvent(std::ifstream& file);

	bool bIsPAL = false;						// Is the machine PAL?
	bool bHasRecording = false;
	EventRecorderStates_e m_state = EventRecorderStates_e::DISABLED;
	void SetState(EventRecorderStates_e _state);

	std::vector<ByteBuffer> v_memSnapshots;	// memory snapshots at regular intervals
	std::vector<SDHREvent> v_events;


	// Replay thread control
	std::thread thread_replay;
	int replay_events_thread(bool* shouldPauseReplay, bool* shouldStopReplay);
	int slowdownMultiplier = 1;		// How much to slow the replay down by
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
