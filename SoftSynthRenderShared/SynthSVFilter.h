#ifndef CSYNTHSVFILTER_H
#define CSYNTHSVFILTER_H

#include "SynthDataPath.h"
#include "SynthObject.h"


class CSynthSVFilter : public CSynthObject
{
	float m_Z1;
	float m_Z2;
public:
	CSynthSVFilter(CSynthParameters *pParams, CSynthObject *pParent);
	virtual ~CSynthSVFilter();
	virtual void Run();
	float Integrator(float fc, float Z, float In);
	float FreqToFc(float freq);
	inline CSynthDataPath* GetIn() { return m_pIn; }
	inline void SetIn(CSynthDataPath* val) { m_pIn = val; }
	inline CSynthDataPath* GetHPo() { return m_pHPo; }
	inline void SetHPo(CSynthDataPath* val) { m_pHPo = val; }
	inline CSynthDataPath* GetBPo() { return m_pBPo; }
	inline void SetBPo(CSynthDataPath* val) { m_pBPo = val; }
	inline CSynthDataPath* GetLPo() { return m_pLPo; }
	inline void SetLPo(CSynthDataPath* val) { m_pLPo = val; }
	inline CSynthDataPath* GetFc() { return m_pFc; }
	inline void SetFc(CSynthDataPath* val) { m_pFc = val; }
	inline CSynthDataPath* GetQ() { return m_pQ; }
	inline void SetQ(CSynthDataPath* val) { m_pQ = val; }

protected:

private:
	CSynthDataPath* m_pIn;		//filter input
	CSynthDataPath* m_pHPo;		//high pass output
	CSynthDataPath* m_pBPo;		//band pass output
	CSynthDataPath* m_pLPo;		//low pass output
	CSynthDataPath* m_pFc;		//frequency control
	CSynthDataPath* m_pQ;		//filter Q control
};

#endif // CSYNTHSVFILTER_H
