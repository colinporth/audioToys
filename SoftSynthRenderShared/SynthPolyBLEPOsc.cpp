#include "stdafx.h"

#include <math.h>
#include "SynthPolyBLEPOsc.h"
#include "SynthCadDefines.h"

CSynthPolyBLEPOsc::CSynthPolyBLEPOsc(CSynthParameters *pParams, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_POLYBLEPSAW, pParams,pParent)
{
	m_pPitch = 0;
	m_pOut = 0;
	m_pPulseWidth = 0;
	m_pWaveSelect = 0;
	m_Phase = 0;
}

CSynthPolyBLEPOsc::~CSynthPolyBLEPOsc()
{
}

void CSynthPolyBLEPOsc::Run(void)
{
	float o;	//output of oscillator
	float Waves[4];
	float pw;
	float freq = m_pPitch->GetData();
	float OscFreqInc = FreqInc(LevelToFreq(freq));
	m_Phase += OscFreqInc;
	while (m_Phase > 1.0)m_Phase -= 1.0;
	Waves[PBOSC_MODE_SAW] = Saw(m_Phase, OscFreqInc);
	if (m_pPulseWidth)
		pw = m_pPulseWidth->GetData();
	else
		pw = 0.5;
	Waves[PBOSC_MODE_PULSE] = Pulse(m_Phase, OscFreqInc, pw);
	Waves[PBOSC_MODE_SINE] = (float)cos(twoPI *  m_Phase);
	Waves[PBOSC_MODE_TRI] = Tri(m_Phase, OscFreqInc);
	//-------------------------------
	// Select waveform.
	//-------------------------------
	float x;
	if (m_pWaveSelect)
		x = m_pWaveSelect->GetData();
	else
		x = float(0.6);	//default to saw
	o = Selector(4, x, Waves);
//	pScope->AddPoint((short int)(32767.0 *o));
	//--------------------------------
	// output
	//--------------------------------
	if(m_pOut) m_pOut->SetData(o);
}

float CSynthPolyBLEPOsc::PolyBlep(float phase, float dphase)
{
	//-------------------------------------------
	// PolyBlep
	//
	// 		This function is looking for a
	// discontinuity.  If it finds one, it returns
	// a correction factor
	// Otherwise it returns 0
	//	Parameters:
	//		phase.........phase of waveform
	//		dphase........change in phase
	//---------------------------------------------
	float rV = 0.0;
	if(phase > 1.0) phase -= 1.0;
	// 0 <= phase < 1
	if (phase < dphase)		//right after the transition
	{
		phase /= dphase;
		// 2 * (phase - phase^2/2 - 0.5)
		rV =  float(phase+phase - phase*phase - 1.0);
	}
	// -1 < phase < 0
	else if (phase > 1.0 - dphase)	//just before the transition
	{
		phase = float((phase - 1.0) / dphase);
		// 2 * (phase^2/2 + phase + 0.5)
		rV = float(phase*phase + phase+phase + 1.0);
	}
	// 0 otherwise
	return rV;
}

float CSynthPolyBLEPOsc::Tri(float phase, float dphase)
{
	//---------------------------------------------
	// Tri
	//		This methode generates a triangle
	// waveform using a polyblep square wave
	// and a leaky integrator.
	//
	//	parameters:
	//		phase..........current phase angle
	//		dphase.........phase increment
	//	Returns:
	//		Value of the triagle wave
	//---------------------------------------------
	float naive_Square;
	if (phase>0.5)
		naive_Square = 1.0;
	else
		naive_Square = -1.0;
	naive_Square -= PolyBlep(phase, dphase);
	naive_Square += PolyBlep((phase + float(0.5)), dphase);
	m_LIz = m_LIz + (naive_Square - m_LIz) * dphase;
	return m_LIz;
}

float CSynthPolyBLEPOsc::Pulse(float phase,
							float dphase,
							float trip
){
	//-----------------------------------------------------
	// Pulse
	//		This methode does the calculations for the
	//	pulse wave. One edge of the pulse is on the
	//	transition between 0 degree phase and 360
	//	degree phase.  The other transistion is
	//	determined by the trip point.
	//
	//	parameters:
	//		phase........phase angle of signal
	//		dphase.......phase increment
	//		trip.........Pulse trip point
	//	Return Value:
	//		Current sample of the pulse wave
	//-------------------------------------------------
	float naive_Pulse;
	if(phase>trip)
		naive_Pulse = 1.0;
	else
		naive_Pulse = -1.0;
	naive_Pulse -= PolyBlep(phase , dphase);
	naive_Pulse += PolyBlep(float(phase + (1.0  - trip)), dphase);
	return naive_Pulse;
}

float CSynthPolyBLEPOsc::Saw(float phase, float dphase)
{
	//--------------------------------------------
	// Saw
	//		This function generates a sawtooth
	// wave.
	//
	//	parameters:
	//		phase.........phase angle of waveform
	//		dphase........change in phase
	//----------------------------------------------
	float naive_saw = float(2.0 * phase - 1.0);
	float o = naive_saw - PolyBlep(phase, dphase);
	return o;
}

float CSynthPolyBLEPOsc::FreqInc(float freq)
{
	return freq/this->GetParams()->GetSampleRate();
}

//-------------------------------
// Windows API
//-------------------------------
bool CSynthPolyBLEPOsc::Create( CSynthObject *pParent)
{
	return CSynthObject::Create(pParent);
}

