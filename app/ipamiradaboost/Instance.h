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

#ifndef DT_INSTANCE_H
#define DT_INSTANCE_H

#include <cstdint>
#include <vector>
#include <string>
#include <cassert>

class Example {

public:
	Example(std::vector<uint8_t> & f, uint8_t c) : features(f), is_positive(c) {};
	size_t n_features() const { return features.size(); }
	bool get_feature(size_t i) const { assert (i < features.size()); return static_cast<bool>(features.at(i)); }
	bool get_class() const { return is_positive; }

private:
	std::vector<uint8_t> features;
	bool is_positive;

};

class Data {

public:
	Data() {};
	Data(std::string filename);
	size_t n_examples() const { return examples.size(); }
	size_t n_features() const { return feature_names.size(); }
	Example get_example(size_t i) const { assert(i < examples.size()); return examples.at(i); }
	std::string get_feature_name(size_t i) const { assert (i < feature_names.size()); return feature_names.at(i); }
	void add_example(Example & e) { examples.push_back(e); }
	void set_feature_names(const Data & another) { feature_names = another.feature_names; }

	using iterator = std::vector<Example>::iterator;
	using const_iterator = std::vector<Example>::const_iterator;

	inline iterator begin() { return examples.begin(); }
	inline const_iterator begin() const { return examples.cbegin(); }
	inline iterator end() { return examples.end(); }
	inline const_iterator end() const { return examples.cend(); }

private:
	std::vector<Example> examples;
	std::vector<std::string> feature_names;

};

#endif
