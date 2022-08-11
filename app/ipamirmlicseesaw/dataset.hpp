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

#ifndef _dataset_hpp_INCLUDED
#define _dataset_hpp_INCLUDED

#include <cassert>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

class DataSample {
  friend class DataSet;

protected:
  std::vector<bool> features{};
  bool isPositive{};

public:
  DataSample(std::vector<bool> &f, uint8_t c) : features(f), isPositive(c) {}
  size_t nFeatures() const { return features.size(); }
  bool getFeature(size_t i) const {
    assert(i < features.size());
    return static_cast<bool>(features[i]);
  }
  bool getClass() const { return isPositive; }
};

class DataSet {
protected:
  std::vector<DataSample> samples{};
  std::vector<std::string> featureNames{};

  size_t nPositive{};

public:
  DataSet();
  DataSet(std::string filename);
  void readFile(std::string filename, char separator = '\0');
  void addNegatedCols(bool force = false);
  size_t nSamples() const { return samples.size(); }
  size_t nFeatures() const { return featureNames.size(); }
  size_t getNPositive() const { return nPositive; }
  size_t getNNegative() const { return samples.size() - nPositive; }
  DataSample getSample(size_t i) const {
    assert(i < samples.size());
    return samples[i];
  }
  std::string getFeatureName(size_t i) const {
    assert(i < featureNames.size());
    return featureNames[i];
  }
  void addSample(DataSample &s) { samples.push_back(s); }
  void removeSample(size_t idx) { samples.erase(samples.begin() + idx); }
  void setFeatureNames(const std::vector<std::string> &names) {
    featureNames = names;
  }

  using iterator = std::vector<DataSample>::iterator;
  using const_iterator = std::vector<DataSample>::const_iterator;

  inline iterator begin() { return samples.begin(); }
  inline const_iterator begin() const { return samples.cbegin(); }
  inline iterator end() { return samples.end(); }
  inline const_iterator end() const { return samples.cend(); }
};

#endif