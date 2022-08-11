/*
 * Author: Christoph Jabs - christoph.jabs@helsinki.fi
 *
 * Copyright © 2022 Christoph Jabs, University of Helsinki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "dataset.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

DataSet::DataSet() {}

DataSet::DataSet(std::string filename) { readFile(filename); }

void DataSet::readFile(std::string filename, char separator) {
  if (separator == '\0')
    separator = ';';

  std::ifstream input{filename};
  if (!input.is_open()) {
    std::cout << "c ERROR: File could not be opened.\n";
    exit(1);
  }
  std::string line;
  getline(input, line);
  std::stringstream header(line);
  while (header.good()) {
    std::string fname;
    getline(header, fname, separator);
    featureNames.push_back(fname);
  }
  featureNames.pop_back();
  while (getline(input, line)) {
    std::stringstream sample(line);
    std::vector<bool> features{};
    while (sample.good()) {
      std::string bit{};
      getline(sample, bit, separator);
      assert(bit == "0" || bit == "1");
      features.push_back(bit == "0" ? false : true);
    }
    bool classIsPos = static_cast<bool>(features.back());
    if (classIsPos)
      nPositive++;
    features.pop_back();
    samples.push_back(DataSample(features, classIsPos));
  }
}

void DataSet::addNegatedCols(bool force) {
  uint32_t fs = featureNames.size();
  for (uint32_t i = 0; i < fs; i++)
    featureNames.push_back("~(" + featureNames[i] + ")");
  for (uint32_t i = 0; i < samples.size(); i++)
    for (uint32_t j = 0; j < fs; j++)
      samples[i].features.push_back(!samples[i].features[j]);
}