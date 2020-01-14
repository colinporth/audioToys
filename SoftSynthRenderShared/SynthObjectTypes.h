#pragma once
#include "SynthAudioOut.h"
#include "SynthBiQuad.h"
#include "synthbiquad_ii.h"
#include "SynthEGadsr.h"
#include "SynthEngine.h"
#include "SynthGlide.h"
#include "SynthMixer.h"
#include "SynthReverb1.h"
#include "SynthSVFilter.h"
#include "SynthVCA.h"

union CSynthObjTypes {
	CSynthObject *m_pSynthObj;
	CSynthAudioOut *m_pSynthAudioOut;
	CSynthBiQuad *m_pSynthBiquad;
	CSynthEGadsr *m_pSynthEGadsr;
	CSynthGlide *m_pSynthGlide;
	CSynthMixer *m_pSynthMixer;
	CSynthReverb1 *m_pSynthReverb;
	CSynthObject *m_pSynthObject;
};