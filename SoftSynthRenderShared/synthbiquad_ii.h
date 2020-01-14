#ifndef CSYNTHBIQUAD_II_H
#define CSYNTHBIQUAD_II_H

#include <math.h>
#include <stdlib.h>
#include "SynthObject.h"
#include "SynthDataPath.h"
#include "SynthParameters.h"
#include "SynthCadDefines.h"


#ifndef M_LN2
#define M_LN2	   0.69314718055994530942
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

class CSynthBiQuadii : public CSynthObject
{
	public:
		CSynthBiQuadii(int type,CSynthParameters *Params, CSynthObject *pParent);
		virtual ~CSynthBiQuadii();
		virtual void Run(void);
		virtual void CalcCoefficients(int type,float freq, float Q);

		CSynthDataPath* GetIn() { return m_pIn; }
		void SetIn(CSynthDataPath* val) { m_pIn = val; }
		CSynthDataPath* GetOut() { return m_pOut; }
		void SetOut(CSynthDataPath* val) { m_pOut = val; }
		CSynthDataPath* GetFreq() { return m_pFreq; }
		void SetFreq(CSynthDataPath* val) { m_pFreq = val; }
		CSynthDataPath* GetQ() { return m_pQ; }
		void SetQ(CSynthDataPath* val) { m_pQ = val; }

	protected:
		float m_b0;		//output filter coefficients (numerator)
		float m_b1;
		float m_b2;
		float m_a1;		//input filter coefficients (denominator)
		float m_a2;
		int m_FilterType;
	private:
		float m_Z1;		//filter state variables
		float m_Z2;
		CSynthDataPath *m_pIn;	//filter input
		CSynthDataPath *m_pOut;	//filter output
		CSynthDataPath *m_pFreq;	//filter frequency control
		CSynthDataPath *m_pQ;		//filter q control
		int m_Dirty;		//indicates that coefficients need recalculating
		float m_LastFreq;
};

#endif // CSYNTHBIQUAD_H
