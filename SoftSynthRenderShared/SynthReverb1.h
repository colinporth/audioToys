#ifndef CSYNTHREVERB1_H
#define CSYNTHREVERB1_H

#include "SynthObject.h"
#include "SynthDataPath.h"

class CSynthReverb1 : public CSynthObject
{
	float *m_pBuff;		//delay buffer
	int m_BuffSize;
	int m_Head;			//head pointer
	int m_Delay;		//delay through buffer
public:
	CSynthReverb1(CSynthParameters *pParams,float Delay, CSynthObject *pParent);
	virtual ~CSynthReverb1();
	virtual void Run();

	CSynthDataPath* GetIn() { return m_pIn; }
	void SetIn(CSynthDataPath* val) { m_pIn = val; }
	CSynthDataPath* GetOut() { return m_pOut; }
	void SetOut(CSynthDataPath* val) { m_pOut = val; }
	CSynthDataPath* GetDelay() { return m_pDelay; }
	void SetDelay(CSynthDataPath* val) { m_pDelay = val; }
	CSynthDataPath* GetFeedBack() { return m_pFeedBack; }
	void SetFeedBack(CSynthDataPath* val) { m_pFeedBack = val; }
	CSynthDataPath* GetMix() { return m_pMix; }
	void SetMix(CSynthDataPath* val) { m_pMix = val; }

protected:

private:
	CSynthDataPath* m_pIn;
	CSynthDataPath* m_pOut;
	CSynthDataPath* m_pDelay;
	CSynthDataPath* m_pFeedBack;
	CSynthDataPath* m_pMix;
};

#endif // CSYNTHREVERB1_H
