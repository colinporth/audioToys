#include "stdafx.h"
#include "SynthCadDefines.h"
#include <math.h>

float NoteToBiLevel(int note)
{
	float level = float(note - 64) / float(64.0);
	return level;

}
float NoteToLevel(int note)
{
	float level = float(note) / float(120.0);
	return level;
}

float LevelToFreq(float level)
{
	///---------------------------------
	/// LevelToFreq
	///		This function converts a
	/// level into its equivalent frequency
	/// where c0 is 16.352Hz
	///----------------------------------
	float freq;

	freq = float(16.352 * pow(TWELTHROOT2,level * 120.0));
	return freq;
}

float FreqToLevel(float freq)
{
	float Level;

	Level =float( log(freq/16.352) / (log(TWELTHROOT2) * 120) );
	return Level;
}

float Selector3(float x, float A, float B, float C)
{
	float y1, y2, y3;
	float o;
	float xAbs = float(fabs(x));
	if (xAbs <= 0.5)
		y1 =float( 4.0 *   x
		- ((xAbs > 0.25) ? (8.0 * (x - ((x >= 0) ? 0.5 : -0.5))
			+ ((x >= 0) ? 2.0 : -2.0)) : 0.0) );
	else
		y1 = 0.0;
	if (xAbs <= 0.75 && xAbs > 0.25)
		y2 = float( 4.0 * (x - ((x >= 0) ? 0.25 : -0.25))
		- ((xAbs > 0.5) ? (8.0 * (x - ((x >= 0) ? 0.75 : -0.75))
			+ ((x >= 0) ? 2.0 : -2.0)) : 0.0) );
	else
		y2 = 0;
	if (xAbs <= 1.0 && xAbs > 0.5)
		y3 = float( 4.0 * (x - ((x >= 0) ? 0.5 : -0.5))
		- ((xAbs > 0.75) ? (8.0 * (x - ((x >= 0) ? 1.0 : -1.0))
			+ ((x >= 0) ? 2.0 : -2.0)) : 0.0) );
	else
		y3 = 0;
	o = A * y1 + B * y2 + C * y3;
	return o;
}

float Selector(int n,float x, float *Inputs)
{
	float o = 0.0;
	float y;
	float xAbs = float(fabs(x));
	bool xgte0 = x >= 0;
	int i;
	float slope = float(n + 1);
	float Interval = float(1.0 / slope);
	float slopeX2 = float( slope * 2.0 );
	float p1, p2, p3;
	p1 = 0.0;
	p2 = Interval;
	p3 = p2 + Interval;

	for (i = 0; i < n; ++i)
	{
		if (xAbs <= p3 && xAbs > p1)
			y = float (slope * (x - (xgte0 ? p1 : -p1))
			- ((xAbs > p2) ? (slopeX2 * (x - (xgte0 ? p3 : -p3))
				+ (xgte0 ? 2.0 : -2.0)) : 0.0) );
		else
			y = 0;
		p1 += Interval;
		p2 += Interval;
		p3 += Interval;
		o += y * Inputs[i];
	}
	return o;
}

