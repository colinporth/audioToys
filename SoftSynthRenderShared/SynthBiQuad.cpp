/* Simple implementation of Biquad filters -- Tom St Denis
 *
 * Based on the work

Cookbook formulae for audio EQ biquad filter coefficients
---------------------------------------------------------
by Robert Bristow-Johnson, pbjrbj@viconet.com  a.k.a. robert@audioheads.com

 * Available on the web at

http://www.smartelectronix.com/musicdsp/text/filters005.txt

 * Enjoy.
 *
 * This work is hereby placed in the public domain for all purposes, whether
 * commercial, free [as in speech] or educational, etc.  Use the code and please
 * give me credit if you wish.
 *
 * Tom St Denis -- http://tomstdenis.home.dhs.org
 *
 * Modified by James Patchell, converted into a C++ class
 *	http://www.noniandjim.com/Jim
*/

#include "stdafx.h"
#include <stdio.h>
#include "SynthBiQuad.h"

CSynthBiQuad::CSynthBiQuad(int type ,CSynthParameters *Params, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_BIQUAD,Params,pParent)
{
	//ctor
	m_x1 = 0.0;
	m_x2 = 0.0;
	m_y2 = m_y1 = 0.0;
	m_pIn = 0;
	m_pOut = 0;
	m_pFreq = 0;
	m_pQ = 0;
	m_Dirty = 0;
	m_FilterType = type;
	m_LastFreq = 0.0;
}

CSynthBiQuad::~CSynthBiQuad()
{
	//dtor
}

/** @brief (one liner)
  *
  * (documentation goes here)
  */
void CSynthBiQuad::Run(void)
{
	float result,sample;
	//---------------------
	// Update Filter Freq
	//---------------------
	if(m_pFreq)
	{
		float newFreq = m_pFreq->GetData();
		if(newFreq != m_LastFreq)
		{
			m_LastFreq = newFreq;
			CalcCoefficients(m_FilterType,LevelToFreq(newFreq),10.0);
		}
	}
	//--------------------
	// Get Input sample
	//---------------------
	sample = m_pIn->GetData();
     /* compute result */
    result = m_b0 * sample + m_b1 * m_x1 + m_b2 * m_x2 -
       m_a1 * m_y1 - m_a2 * m_y2;

    /* shift x1 to x2, sample to x1 */
    //Numerator
   m_x2 = m_x1;
   m_x1 = sample;

    /* shift y1 to y2, result to y1 */
    // denominator
   m_y2 = m_y1;
   m_y1 = result;
   m_pOut->SetData(result);
}

/** @brief (one liner)
  *
  * (documentation goes here)
  */
void CSynthBiQuad::CalcCoefficients(int type,float freq, float Q)
{
    float A, omega, sn, cs, alpha, beta;
    float a0, a1, a2, b0, b1, b2;
	float srate = (float)this->GetParams()->GetSampleRate();
     /* setup variables */
	a0 = a1 = a2 = b0 = b1 = b2 = 0.0;
    A = 1.0;
    omega = float(2.0 * M_PI * freq /srate);
    sn = (float)sin(omega);
    cs = (float)cos(omega);
    alpha = float(sn/(2.0 * Q));
    beta = (float) sqrt(A + A);

    switch (type) {
    case BIQUAD_LPF:
        b0 = (1 - cs) /2;
        b1 = 1 - cs;
        b2 = (1 - cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BIQUAD_HPF:
        b0 = (1 + cs) /2;
        b1 = -(1 + cs);
        b2 = (1 + cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BIQUAD_BPF:
        b0 = alpha;
        b1 = 0;
        b2 = -alpha;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BIQUAD_NOTCH:
        b0 = 1;
        b1 = -2 * cs;
        b2 = 1;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BIQUAD_PEQ:
        b0 = 1 + (alpha * A);
        b1 = -2 * cs;
        b2 = 1 - (alpha * A);
        a0 = 1 + (alpha /A);
        a1 = -2 * cs;
        a2 = 1 - (alpha /A);
        break;
    case BIQUAD_LSH:
        b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
        a0 = (A + 1) + (A - 1) * cs + beta * sn;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - beta * sn;
        break;
    case BIQUAD_HSH:
        b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
        a0 = (A + 1) - (A - 1) * cs + beta * sn;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - beta * sn;
        break;
     }

    /* precompute and normallize the coefficients */
    m_b0 = b0 /a0;	//numerator coefficients
    m_b1 = b1 /a0;
    m_b2 = b2 /a0;
    m_a1 = a1 /a0;	//denominator coefficients
    m_a2 = a2 /a0;
//	printf("B0=%lf B1=%lf B2=%lf A1=%lf A2=%lf",m_b0,m_b1,m_b2,m_a1,m_a2);
}



