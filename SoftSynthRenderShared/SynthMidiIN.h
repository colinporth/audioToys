#pragma once
#include "SynthObject.h"
#include "SynthDataPath.h"
#include <mmsystem.h>

struct MsgReg {
	int m_Trigger;	//value that triggers sending of message
	int m_Msg;		//Message to send
//	CWinThread *m_pQ;	//handle of object to send it to
	MsgReg *m_pNext;	//pointer to next entry
	MsgReg() { m_pNext = 0; }
	~MsgReg() {}
};

struct DoubleReg {
	int Handle;			//tells which output to send from
	float *m_pValue;	//value to send to
//	CWinThread *m_pQ;	//where to send a message to
	int m_Msg;			//message value to send
	DoubleReg *m_pNext;	//next registry
	DoubleReg() { m_pNext = 0; }
	~DoubleReg() {};
};
class CSynthMidiIN :public CSynthObject
{
public:
	MsgReg *m_pHead;
	MsgReg *m_pEnd;
	DoubleReg *m_pDRHead;
	DoubleReg *m_pDREnd;

	static DWORD MidiWorker(LPVOID pP);
	HANDLE m_WorkerThread;
	int m_ThreadLoop;
	//--------------------------
	// Outputs
	//--------------------------
	CSynthDataPath *m_pPitchOut;
	CSynthDataPath *m_pVelocityOut;
	CSynthDataPath *m_pPresureOut;
	CSynthDataPath *m_pPitchBend;
	CSynthDataPath *m_pGateOut;
	HMIDIIN m_hMidiIn;	//handle to midi channel
	MIDIHDR m_MidiIn1;
	MIDIHDR m_MidiIn2;
	int m_CurrentNote;	//current active note
	int m_Gate;			//gate state
	int m_Vel;
	int m_Pres;
	int m_PitchBend;

public:
	CSynthMidiIN(CSynthParameters *pParams, CSynthObject *pParent);
	virtual ~CSynthMidiIN();
	//-------------------------------
	// sound generating functions
	//-------------------------------
	virtual void Run(void);
	void RegisterDouble(int trig, float *pD,int Msg);
	void CheckDoubleRegister(int trig, float value);
	//------------------------------
	// Windows API
	//------------------------------
	virtual bool Create(CSynthObject *pParent);
	//-------------------------------
	// Getter Functions
	//-------------------------------
	inline void SetPitchData(CSynthDataPath *pDP) { m_pPitchOut = pDP; }
	inline void SetVelData(CSynthDataPath *pDP) { m_pVelocityOut = pDP; }
	inline void SetPresureData(CSynthDataPath *pDP) { m_pPresureOut = pDP; }
	inline void SetPBendData(CSynthDataPath *pDP) { m_pPitchBend = pDP; }
	inline void SetGateData(CSynthDataPath *pDP) { m_pGateOut = pDP; }
	void DecodeMidiMessageShort(int Chan, int Cmd, int Note, int Vel);
	int KillThead();
	inline void SetThread(HANDLE hT) { m_WorkerThread = hT; }
};

