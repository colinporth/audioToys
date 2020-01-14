#ifndef CSYNTHPARAMETERS_H
#define CSYNTHPARAMETERS_H

#include "SynthCadDefines.h"

class CSynthParameters
{
	private:
		float m_FreqRad;
		int m_SampleRate;	//samples per second
		int m_MidiChannel;	//midi channel to respond to
		int m_ButtonChanel;	//midi channel buttons are on
		int m_SamplesPerBlock;	//number of samples to generate at a time
	public:
		CSynthParameters();
		virtual ~CSynthParameters();

		inline float GetFreqRad(void){return m_FreqRad;}
		inline int GetSampleRate() { return m_SampleRate; }
		inline void SetSampleRate(int val) {
			m_SampleRate = val;
			m_FreqRad = (float)twoPI / (float)val;
		}
		inline void SetMidiCh(int ch) { m_MidiChannel = ch; }
		inline int GetMidiCh(void) { return m_MidiChannel; }
		inline void SetButtonChan(int ch) { m_ButtonChanel = ch; }
		inline int GetButtonChan(void){return m_ButtonChanel; }
		inline void SetSamplesPerBlock(int n) { m_SamplesPerBlock = n; }
		inline int GetSamplesPerBlock() { return m_SamplesPerBlock; }
	protected:

};

#endif // CSYNTHPARAMETERS_H
