#ifndef CSYNTHGLIDE_H
#define CSYNTHGLIDE_H

#include "SynthDataPath.h"
#include "SynthObject.h"


class CSynthGlide : public CSynthObject
{
	public:
		CSynthGlide(CSynthParameters *params, CSynthObject *pParent);
		virtual ~CSynthGlide();
		virtual void Run(void);
		float ConvertRate(float v);
		CSynthDataPath *GetIn() { return m_pIn; }
		void SetIn(CSynthDataPath *val) { m_pIn = val; }
		CSynthDataPath *GetOut() { return m_pOut; }
		void SetOut(CSynthDataPath *val) { m_pOut = val; }
		CSynthDataPath *GetUpRate() { return m_pUpRate; }
		void SetUpRate(CSynthDataPath *val) { m_pUpRate = val; }
		CSynthDataPath *GetDownRate() { return m_pDownRate; }
		void SetDownRate(CSynthDataPath *val) { m_pDownRate = val; }

	protected:

	private:
		float m_Goal;
		float m_Z;
		CSynthDataPath *m_pIn;
		CSynthDataPath *m_pOut;
		CSynthDataPath *m_pUpRate;
		CSynthDataPath *m_pDownRate;
};

#endif // CSYNTHGLIDE_H
