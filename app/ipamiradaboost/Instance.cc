//  Copyright (c) 2022 Andreas Niskanen, University of Helsinki
//
//  Permission is hereby granted, free of charge, to any person obtaining a 
//  copy of this software and associated documentation files (the "Software"), 
//  to deal in the Software without restriction, including without limitation 
//  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
//  and/or sell copies of the Software, and to permit persons to whom the 
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in 
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
//  DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

#include "Instance.h"

using namespace std;

Data::Data(string filename)
{
	ifstream input(filename);
	string line; getline(input, line);
	stringstream header(line);
	while (header.good()) {
		string fname;
		getline(header, fname, ',');
		feature_names.push_back(fname);
	}
	feature_names.pop_back();
	while (getline(input, line)) {
		stringstream example(line);
		vector<uint8_t> features;
		while (example.good()) {
			string bit;
			getline(example, bit, ',');
			assert(bit == "0" || bit == "1");
			features.push_back(bit == "0" ? 0 : 1);
		}
		bool class_is_pos = static_cast<bool>(features.back());
		features.pop_back();
		examples.push_back(Example(features, class_is_pos));
	}
}
