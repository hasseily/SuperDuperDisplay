#pragma once

#ifndef CYCLECOUNTER_H
#define CYCLECOUNTER_H

#include <stdint.h>
#include <stddef.h>

class CycleCounter
{
public:
	void IncrementCycles(int inc, bool isVBL);
	bool IsVBL();
	bool IsHBL();
	bool IsInBlank();
	uint32_t GetScanline();
	uint32_t GetByteYPos();
	uint32_t m_vbl_start = 0;	// debug to know when we think vbl started previously

	bool isSHR = false;

	// public singleton code
	static CycleCounter* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new CycleCounter();
		return s_instance;
	}
private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static CycleCounter* s_instance;
	CycleCounter()
	{
		Initialize();
	}
};

#endif // CYCLECOUNTER_H