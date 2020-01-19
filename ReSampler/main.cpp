//{{{
/*
* Copyright (C) 2016 - 2020 Judd Niemann - All Rights Reserved.
* You may use, distribute and modify this code under the
* terms of the GNU Lesser General Public License, version 2.1
*
* You should have received a copy of GNU Lesser General Public License v2.1
* with this file. If not, please refer to: https://github.com/jniemann66/ReSampler
*/
//}}}
// main.cpp : defines main entry point

#include <iostream>
#include <string>

#include "ReSampler.h"

int main (int argc, char * argv[]) {
	int result = ReSampler::runCommand (argc, argv);
	return result;
	}
