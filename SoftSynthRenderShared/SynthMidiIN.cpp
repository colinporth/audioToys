//{{{
#include "stdafx.h"
#include <mmsystem.h>
#include "SynthMidiIN.h"
#include "SynthCadDefines.h"
#include "SynthEngine.h"
//}}}

//#define MIDIMSG_MIDI_IN   0x100
//{{{
DWORD CSynthMidiIN::MidiWorker(LPVOID pP)
{
	int Cmd;
	int Vel;
	int Note;
	int Chan;
	CSynthMidiIN *pSMI = (CSynthMidiIN *)pP;
	UINT rV = 0;

	while (pSMI->m_ThreadLoop)
	{
		MSG message;

		GetMessage(&message, NULL, 0, 0);
		switch (message.message)
		{
			case MM_MIM_DATA:
//      case MIDIMSG_MIDI_IN:
				Chan = CHAN(message.lParam);
				Note = NOTE(message.lParam);
				Vel = VEL(message.lParam);
				Cmd = CMD(message.lParam);
				pSMI->DecodeMidiMessageShort(Chan, Cmd, Note, Vel);
				break;
			case MM_MIM_LONGERROR:
				break;
			case MM_MIM_MOREDATA:
				break;
			case MM_MIM_OPEN:
				break;
			case MM_MIM_CLOSE:
				break;
			case MSG_KILLTHREAD:
				pSMI->m_ThreadLoop = 0;
				break;
			default:
				DispatchMessage(&message);
				break;
		}
	}
	return rV;

}
//}}}
//{{{
CSynthMidiIN::CSynthMidiIN(CSynthParameters *pParams, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_MIDI,pParams,pParent)
{
	m_hMidiIn = 0;
	m_pHead = 0;
	m_pEnd = 0;
	m_pDRHead = 0;
	m_pDREnd = 0;
}
//}}}

//{{{
CSynthMidiIN::~CSynthMidiIN()
{
//  if (m_hMidiIn) midiInClose(m_hMidiIn);
}
//}}}

//{{{
bool CSynthMidiIN::Create(CSynthObject *pParent)
{
	//--------------------------------------
	// Create
	//  Among other things, this is where
	// we open up the desired midi channel
	//
	//---------------------------------------
	if (false == CSynthObject::Create( pParent))
		return false;


	HANDLE hMidiWorker;
	hMidiWorker = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE) CSynthMidiIN::MidiWorker, this, 0, NULL);

	SetThread(hMidiWorker);
	m_ThreadLoop = 1;
	MMRESULT r = midiInOpen(&m_hMidiIn, 0, (DWORD_PTR)GetThreadId(hMidiWorker),(DWORD_PTR)this, CALLBACK_THREAD);
	if (MMSYSERR_NOERROR == r)
	{
		printf("Midi Opened\n");
		midiInPrepareHeader(m_hMidiIn, &m_MidiIn1, sizeof(MIDIHDR));
		midiInPrepareHeader(m_hMidiIn, &m_MidiIn2, sizeof(MIDIHDR));
		m_MidiIn1.lpData = new char[4096];
		m_MidiIn2.lpData = new char[4096];
		m_MidiIn1.dwBufferLength = 4096;
		m_MidiIn2.dwBufferLength = 4096;
		midiInStart(m_hMidiIn);
	}
	else if(MMSYSERR_NODRIVER == r)
		printf("Midi ERROR No Driver\n");
	else
		printf("Midi ERROR\n");

	return true;
}
//}}}
//{{{
void CSynthMidiIN::Run(void)
{

}
//}}}

//{{{
void CSynthMidiIN::RegisterDouble(int trig, float *pD,  int Msg)
{
	DoubleReg *pDR = new DoubleReg;
	pDR->Handle = trig;
	pDR->m_pValue = pD;
	pDR->m_Msg = Msg;
	if (m_pDRHead == NULL)
	{
		m_pDRHead = pDR;
		m_pDREnd = m_pDRHead;
	}
	else
	{
		m_pDREnd->m_pNext = pDR;
		m_pDREnd = pDR;
	}

}
//}}}
//{{{
void CSynthMidiIN::CheckDoubleRegister(int trig, float value)
{
	DoubleReg *pDR = m_pDRHead;
	while (pDR)
	{
		if (trig == pDR->Handle)
		{
			*pDR->m_pValue = float(value)/(float)128.0;
//      pDR->m_pQ->PostThreadMessageW(pDR->m_Msg, 0, 0);
		}
		pDR = pDR->m_pNext;
	}
}
//}}}

//{{{
void CSynthMidiIN::DecodeMidiMessageShort(int Chan, int Cmd, int Note, int Vel)
{
	static int NoteCountDown = 0;
	static int NoteQueue[20];
	int Value;
	DWORD SEID = ((CSynthEngine *)GetParent())->GetID();
	switch (Cmd)
	{
	case MIDI_NOTEOFF:
//    printf("Note Off\n");
		if (Chan == GetParams()->GetMidiCh()) //our midi channel?
		{
			--NoteCountDown;
			m_Vel = Vel;
//      printf("Message Sent\n");
			if(NoteCountDown)
				PostThreadMessageW(SEID, WM_NOTE_ON, NoteQueue[NoteCountDown], Vel);
			else
			{
				PostThreadMessageW(SEID, WM_NOTE_ON, NoteQueue[NoteCountDown], Vel);
				m_Gate = 0;
				PostThreadMessageW(SEID, WM_ADSR_GATE, m_Gate, 0);
			}

		}
		else if (Chan == GetParams()->GetButtonChan())
		{
		}
		break;
	case MIDI_NOTEON:
		printf("Note On %d\n", NoteCountDown);
		if (Chan == GetParams()->GetMidiCh()) //our midi channel?
		{
			NoteQueue[NoteCountDown++] = Note;
			m_CurrentNote = Note;
			m_Gate = 1;
			m_Vel = Vel;
			PostThreadMessageW(SEID, WM_NOTE_ON, Note, Vel);
			if(NoteCountDown < 2)
				PostThreadMessageW(SEID, WM_ADSR_GATE, m_Gate, 0);
		}
		else if (Chan == GetParams()->GetButtonChan())
		{
		}
		break;
	case MIDI_POLYPRESS:
		break;
	case MIDI_CTRLCHNG:
		if (Chan == GetParams()->GetMidiCh())
		{
//      CheckButtonMessages(Note, Vel);
			switch (Note)
			{
				case LK25_KNOB_TOP1:  //Attack
					PostThreadMessageW(SEID, WM_LK25_KNOB_1, Note, Vel);
					break;
				case LK25_KNOB_TOP2:  //Decay
					PostThreadMessageW(SEID, WM_LK25_KNOB_2, Note, Vel);
					break;
				case LK25_KNOB_TOP3:  //Sustain
					PostThreadMessageW(SEID, WM_LK25_KNOB_3, Note, Vel);
					break;
				case LK25_KNOB_TOP4:  //Release
					PostThreadMessageW(SEID, WM_LK25_KNOB_4, Note, Vel);
					break;
				case LK25_KNOB_TOP5:  //filter Q
					PostThreadMessageW(SEID, WM_LK25_KNOB_5, Note, Vel);
					break;
				case LK25_KNOB_TOP6:  //filter F
					PostThreadMessageW(SEID, WM_LK25_KNOB_6, Note, Vel);
					break;
				case LK25_KNOB_TOP7:  //pwm
					PostThreadMessageW(SEID, WM_LK25_KNOB_7, Note, Vel);
					break;
				case LK25_KNOB_TOP8:
					PostThreadMessageW(SEID, WM_LK25_KNOB_8, Note, Vel);
					break;
			}
		}
		break;
	case MIDI_CHNLPRESS:
		break;
	case MIDI_PITCHBEND:
		Value = Vel;
		break;
	case MIDI_PGMCHANGE:
		break;
	case MIDI_CLOCK:
		break;
	case MIDI_START:
		break;
	case MIDI_STOP:
		break;
	case MIDI_CONTINUE:
		break;
	}
}
//}}}

//{{{
int CSynthMidiIN::KillThead()
{
	PostThreadMessageW(GetThreadId(m_WorkerThread), MSG_KILLTHREAD, 0, 0);
	if (m_WorkerThread)
	{
		WaitForSingleObject(m_WorkerThread, INFINITE);

		CloseHandle(m_WorkerThread);
		m_WorkerThread = NULL;
	}

	return 0;
}
//}}}
