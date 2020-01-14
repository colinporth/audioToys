#ifndef CSYNTHMIXER_H
#define CSYNTHMIXER_H

#include "SynthDataPath.h"
#include "SynthObject.h"


class CSynthMixer : public CSynthObject
{
	public:
		CSynthMixer(int nChannels,CSynthParameters *params, CSynthObject *pParent);
		virtual ~CSynthMixer();
		virtual void Run();
		inline CSynthDataPath *GetIn(int ch) { return m_ppIn[ch]; }
		inline void SetIn(int ch,CSynthDataPath *val) { m_ppIn[ch] = val; }
		inline CSynthDataPath *GetControl(int ch) { return m_ppControl[ch]; }
		inline void SetControl(int ch,CSynthDataPath *val) { m_ppControl[ch] = val; }
		inline CSynthDataPath *GetOut() { return m_pOut; }
		inline void SetOut(CSynthDataPath *val) { m_pOut = val; }

	protected:

	private:
		int m_nChannels;
		CSynthDataPath **m_ppIn;
		CSynthDataPath **m_ppControl;
		CSynthDataPath *m_pOut;
};

#endif // CSYNTHMIXER_H
