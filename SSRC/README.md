
  SSRC : A fast and high quality sampling rate converter
                                           written by Naoki Shibata


Homepage : http://shibatch.sourceforge.net/
e-mail   : shibatch@users.sourceforge.net


This program converts sampling rates of PCM wav files. This
program also has a function to apply dither to its output and extend
perceived dynamic range.

Sampling rates of 44.1kHz and 48kHz are populary used, but the ratio
between these two frequencies is 147:160, which are not small numbers.
As a result, sampling rate conversion without degradation of sound
quality requires filter with very large order, and it is difficult to
have both quality and speed. This program quickly converts between
these sampling frequencies without audible degradation.
