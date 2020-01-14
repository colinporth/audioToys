#ifndef POLYBLEPOSC_H
#define POLYBLEPOSC_H

#include "SynthObject.h"
#include "SynthDataPath.h"

enum OscMode {
	PBOSC_MODE_SINE,
	PBOSC_MODE_TRI,
	PBOSC_MODE_SAW,
	PBOSC_MODE_PULSE,
};

class CSynthPolyBLEPOsc : public CSynthObject
{
	COLORREF m_bkColor;
	CSynthDataPath *m_pOut;	//oscillator output
	CSynthDataPath *m_pPitch;	//pitch control
	CSynthDataPath *m_pPulseWidth;	//pulse width control
	CSynthDataPath *m_pWaveSelect;
	OscMode m_Mode;	//waveform mode
	float m_Phase;	//phase accumulator
	float m_y1, m_y2, m_y3;
	float m_LIz;	//leaky integrator state variable
public:
	//--------------------------------------------
	// Getter functions
	//--------------------------------------------
	inline void SetOutput(CSynthDataPath *pDP){m_pOut = pDP;}
	inline CSynthDataPath *GetOutput(void){return m_pOut;}
	inline void SetPitch(CSynthDataPath *pDP){m_pPitch = pDP;}
	inline CSynthDataPath *GetPitch(void){return m_pPitch;}
	inline void SetPW(CSynthDataPath *pDP){m_pPulseWidth = pDP;}
	inline CSynthDataPath *GetPW(){return m_pPulseWidth;}
	inline void SetOscMode(OscMode m){m_Mode = m;}
	inline void SetWaveSel(CSynthDataPath *pDP) { m_pWaveSelect = pDP; }
	//---------------------------------------------
	// Implementation
	//---------------------------------------------
	CSynthPolyBLEPOsc(CSynthParameters *pParams, CSynthObject *pParent);
	virtual ~CSynthPolyBLEPOsc();
	virtual void Run();
	virtual bool Create(CSynthObject *pParent);
private:
	//----------------------------------------------
	// Oscillator functions
	//----------------------------------------------
	float PolyBlep(float phase, float dphase);
	float Saw(float phase, float dphase);
	float Pulse(float phase,float dphase,float trip);
	float Tri(float phase, float dphase);
	float FreqInc(float freq);
};

#endif // POLYBLEPOSC_H
