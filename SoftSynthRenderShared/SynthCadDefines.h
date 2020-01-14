#pragma once
extern float NoteToLevel(int note);
extern float LevelToFreq(float level);
extern float FreqToLevel(float freq);
extern float Selector3(float x, float A, float B, float C);
extern float Selector(int n, float x, float *Inputs);
extern float NoteToBiLevel(int note);
//----------------------------------
// Midi channel Messages
//----------------------------------
#define MIDI_NOTEOFF	(0x80)
#define MIDI_NOTEON		(0x90)
#define MIDI_POLYPRESS	(0xa0)
#define MIDI_CTRLCHNG	(0xb0)
#define MIDI_PGMCHANGE	(0xc0)
#define MIDI_CHNLPRESS	(0xd0)
#define MIDI_PITCHBEND	(0xe0)
//-------------------------------------
// Submessages for MIDI_CTRLCHNG
//-------------------------------------

#define MIDI_CTRLCHNG_ALLOFF	(120)
#define MIDI_CTRLCHNG_RESETALL	(121)
#define MIDI_CTRLCHNG_LOCAL		(122)
#define MIDI_CTRLCHNG_LOCAL_OFF	(0)		//sub message to MIDI_CTRLCHNG_LOCAL
#define MIDI_CTRLCHNG_LOCAL_ON	(127)	//sub message to MIDI_CTRLCHNG_LOCAL
#define MIDI_CTRLCHNG_NOTESOFF	(123)
#define MIDI_CTRLCHNG_OMNION	(124)
#define MIDI_CTRLCHNG_OMNIOFF	(125)
#define MIDI_CTRLCHNG_MONO_ON	(126)
#define MIDI_CTRLCHNG_POLY_ON	(127)

//-------------------------------------
// Midi Real Time Messages
//-------------------------------------
#define MIDI_CLOCK		(0xf8)
#define MIDI_START		(0xfa)
#define MIDI_STOP		(0xfc)
#define MIDI_CONTINUE	(0xfb)
//-------------------------------------
// Midi System Exclusive
//--------------------------------------
#define MIDI_SYSEX		(0xf0)
#define MIDI_SYSEXEND	(0xf7)

//---------------------------------------
// LaunchKey 25 defines
//---------------------------------------
// Note ON/OFF Midi CH 9
//---------------------------------------
#define LK25_BUTTON_TOP1			40
#define LK25_BUTTON_TOP2			41
#define LK25_BUTTON_TOP3			42
#define LK25_BUTTON_TOP4			43
#define LK25_BUTTON_TOP5			48
#define LK25_BUTTON_TOP6			49
#define LK25_BUTTON_TOP7			50
#define LK25_BUTTON_TOP8			51

#define LK25_BUTTON_BOT1			36
#define LK25_BUTTON_BOT2			37
#define LK25_BUTTON_BOT3			38
#define LK25_BUTTON_BOT4			39
#define LK25_BUTTON_BOT5			44
#define LK25_BUTTON_BOT6			45
#define LK25_BUTTON_BOT7			46
#define LK25_BUTTON_BOT8			47

#define LK25_CH9_BUTTONS_BASEVALUE		0x100
//---------------------------------------
// Control Change Midi Ch 0
//---------------------------------------

#define LK25_BUTTONRND_TOP			104
#define LK25_BUTTONRND_BOT			105
#define LK25_KNOB_TOP1				21
#define LK25_KNOB_TOP2				22
#define LK25_KNOB_TOP3				23
#define LK25_KNOB_TOP4				24
#define LK25_KNOB_TOP5				25
#define LK25_KNOB_TOP6				26
#define LK25_KNOB_TOP7				27
#define LK25_KNOB_TOP8				28

#define LK25_SLIDER					7
#define LK25_BUTTON_REW				112
#define LK25_BUTTON_FFW				113
#define LK25_BUTTON_PAUSE			114
#define LK25_BUTTON_PLAY			115
#define LK25_BUTTON_CIRC			116
#define LK25_BUTTON_REC				117

#define LK25_WHEEL_PBND				256		//there is only one PB
#define LK25_WHEEL_MOD				1
//---------------------------------------
// Important constants
//----------------------------------------
#define twoPI 6.283185307179586476925286766559
#define PI	3.1415926535897932384626433832795
#define OneOverTwoPi	0.15915494309189533576888376337251
#define OneOverPI	0.31830988618379067153776752674503
#define TWELTHROOT2	1.0594630943592952645618252949463
#define ONEOVERLNTWELTHROOT2	17.312340490667560888319096172023
extern float LevelToFreq(float l);
extern float FreqToLevel(float freq);

/* filter types */
enum {
	BIQUAD_LPF, /* low pass filter */
	BIQUAD_HPF, /* High pass filter */
	BIQUAD_BPF, /* band pass filter */
	BIQUAD_NOTCH, /* Notch Filter */
	BIQUAD_PEQ, /* Peaking band EQ filter */
	BIQUAD_LSH, /* Low shelf filter */
	BIQUAD_HSH /* High shelf filter */
};

//-----------------------------------
// Keyword Tokens
//-----------------------------------

enum Tokens {
	TOKEN_NONE = 0,
	TOKEN_NUM = 256,
	TOKEN_STRING,
	TOKEN_VCO,
	TOKEN_RED,
	TOKEN_GREEN,
	TOKEN_BLUE

};

//-----------------------------------------------------
// Save flags
//-----------------------------------------------------
#define CADOBJECT_SAVE_ADDCOMMA_ATEND		1
//-----------------------------------------------------
#define MAX_ZOOM		7	//maximum number of zoom levels

#define RED(x)		((x & 0xff0000) >> 16)
#define GREEN(x)	((x & 0xff00) >> 8)
#define BLUE(x)		(x & 0x0ff)

#define CMD(x)		(x & 0x0f0)
#define CHAN(x)		(x & 0x0f)
#define NOTE(x)		((x & 0x0ff00)>>8)
#define VEL(x)		((x & 0x0ff0000) >> 16)

//--------------------------------------
// CSynthObject types
//--------------------------------------
enum SynthObjectTypes {
	OBJECT_TYPE_NONE,
	OBJECT_TYPE_DATAPATH,
	OBJECT_TYPE_WAVOUTPUT,
	OBJECT_TYPE_BIQUAD,
	OBJECT_TYPE_POLYBLEPSAW,
	OBJECT_TYPE_EGADSR,
	OBJECT_TYPE_VCA,
	OBJECT_TYPE_GLIDE,
	OBJECT_TYPE_MIXER,
	OBJECT_TYPE_SVFILTER,
	OBJECT_TYPE_REVERB1,
	OBJECT_TYPE_AUDIO,
	OBJECT_TYPE_MIDI,
	OBJECT_TYPE_SLIDER,
	OBJECT_TYPE_KNOB,
	OBJECT_TYPE_LED,
	OBJECT_TYPE_ENGINE,
	OBJECT_TYPE_COREAUDIO
};

//--------------------------------------------
// DRAWSTATE defines
//--------------------------------------------
enum enumDRAWSTATE {
	DRAWSTATE_SET_ATTRIBUTES,
	DRAWSTATE_WAITFORMOUSE_DOWN,
	DRAWSTATE_MOVE,
	DRAWSTATE_PLACE,
	DRAWSTATE_PLACE_AUTO,
	DRAWSTATE_ARCSTART,
	DRAWSTATE_ARCEND,
	DRAWSTATE_GETREFEREMCE,
	DRAWSTATE_DRAG,
	DRAWSTATE_SELECT,	//nothing is selected
	DRAWSTATE_SELECT_MOVE,
	DRAWSTATE_SELECT_COPY,
};

//--------------------------------------------
// DRAW mode defines
//--------------------------------------------
enum enumOBJECTMODE {
	OBJECT_MODE_FINAL = 0,
	OBJECT_MODE_SELECTED,
	OBJECT_MODE_SKETCH,
	OBJECT_MODE_ARCSTART,
	OBJECT_MODE_ARCEND
};

enum enumDRAWMODE {
	DRAWMODE_SELECT = 0,
	DRAWMODE_MOVE,
	DRAWMODE_COPY,
	DRAWMODE_CUT,
	DRAWMODE_PASTE,
	DRAWMODE_SELECTREGION,
};


//-------------------------------------------
// User Defined Messages
//-------------------------------------------

#define WM_STOPAUTOSCROLL		WM_USER+1

//-----------------------
// Midi IN
//-----------------------

#define WM_MIDIIN				WM_USER+2
#define WM_MIDIIN_LONG			WM_USER+3
#define WM_LK25_KNOB_1			WM_USER+25
#define WM_LK25_KNOB_2			WM_USER+26
#define WM_LK25_KNOB_3			WM_USER+27
#define WM_LK25_KNOB_4			WM_USER+28
#define WM_LK25_KNOB_5			WM_USER+29
#define WM_LK25_KNOB_6			WM_USER+30
#define WM_LK25_KNOB_7			WM_USER+31
#define WM_LK25_KNOB_8			WM_USER+32
#define WM_NOTE_OFF				WM_USER+33
#define WM_NOTE_ON				WM_USER+34
//------------------------
// Audio out
//------------------------
#define WM_WAVEFORM_DONE		WM_USER+4
#define WM_WAVEFORM_OPEN		WM_USER+5
#define WM_WAVEFORM_CLOSE		WM_USER+6
#define WM_WAVEFORM_WRITEBLOCK	WM_USER+21
//------------------------
// Synth Engine
//------------------------
// this message tells the
// synth engine it is to
// fill a wave buffer
// then send it out
//-------------------------
#define WM_NEXTWAVEBLOCK		WM_USER+7

//------------------------
// Oscillator
//------------------------

#define WM_OSC_PITCH			WM_USER+8
#define WM_OSC_BEND				WM_USER+20
#define WM_OSC_PULSEWIDTH		WM_USER+9
#define WM_OSC_WAVESEL			WM_USER+10

//-------------------------
// Biquad Filter
//-------------------------

#define WM_BIQUAD_FREQ			WM_USER+11
#define WM_BIQUAD_Q				WM_USER+12
#define WM_BIQUAD_SHAPE			WM_USER+13

//-------------------------
// Glide
//-------------------------

#define WM_GLIDE_RATEUP			WM_USER+14
#define WM_GLIDE_RATEDOWN		WM_USER+15

//-------------------------
// ADSR EnvelopeGenerator
//-------------------------

#define WM_ADSR_ATTACK			WM_USER+16
#define WM_ADSR_DECAY			WM_USER+17
#define WM_ADSR_SUSTAIN			WM_USER+18
#define WM_ADSR_RELEASE			WM_USER+19
#define WM_ADSR_GATE			WM_USER+35

//------------------------
// LED messages
//------------------------

#define WM_LEDSTATE				WM_USER+22

//-------------------------
// Main View
//------------------------

#define WM_INVALIDATE			WM_USER+23
#define WM_DISPLAYSCOPE			WM_USER+24
//next up WM_USER+36

//----------------------------------
// My own home grown messages
//----------------------------------

enum msgqueueMSG {
	MSG_KILLTHREAD,
	MSG_BUTTON_TOP1,
	MSG_REPAINT
};

enum moduleIDs {
	MODID_LED,
	MODID_MIDIIN
};

enum outputIDs {
	OUTID_MIDI_PITCH,
	OUTID_MIDI_VEL,
	OUTID_MIDI_GATE,
	OUTID_LK25_BUTTON_TOP1
};


