/*
 *  FeatureData.cpp
 *  mert - Minimum Error Rate Training
 *
 *  Created by Nicola Bertoldi on 13/05/08.
 *
 */

#include "FeatureData.h"

#include <limits>
#include "FileStream.h"
#include "Util.h"
#include <cstdio>

static const float MIN_FLOAT=-1.0*numeric_limits<float>::max();
static const float MAX_FLOAT=numeric_limits<float>::max();

FeatureData::FeatureData()
    : m_num_features(0),
      m_sparse_flag(false) {}

void FeatureData::save(std::ofstream& outFile, bool bin)
{
  for (featdata_t::iterator i = m_array.begin(); i != m_array.end(); i++)
    i->save(outFile, bin);
}

void FeatureData::save(const std::string &file, bool bin)
{
  if (file.empty()) return;

  TRACE_ERR("saving the array into " << file << std::endl);

  std::ofstream outFile(file.c_str(), std::ios::out); // matches a stream with a file. Opens the file

  save(outFile, bin);

  outFile.close();
}

void FeatureData::load(ifstream& inFile)
{
  FeatureArray entry;

  while (!inFile.eof()) {

    if (!inFile.good()) {
      std::cerr << "ERROR FeatureData::load inFile.good()" << std::endl;
    }

    entry.clear();
    entry.load(inFile);

    if (entry.size() == 0)
      break;

    if (size() == 0)
      setFeatureMap(entry.Features());

    if (entry.hasSparseFeatures())
      m_sparse_flag = true;

    add(entry);
  }
}


void FeatureData::load(const std::string &file)
{
  TRACE_ERR("loading feature data from " << file << std::endl);

  inputfilestream inFile(file); // matches a stream with a file. Opens the file

  if (!inFile) {
    throw runtime_error("Unable to open feature file: " + file);
  }

  load((ifstream&) inFile);

  inFile.close();
}

void FeatureData::add(FeatureArray& e)
{
  if (exists(e.getIndex())) { // array at position e.getIndex() already exists
    //enlarge array at position e.getIndex()
    size_t pos = getIndex(e.getIndex());
    m_array.at(pos).merge(e);
  } else {
    m_array.push_back(e);
    setIndex();
  }
}

void FeatureData::add(FeatureStats& e, const std::string& sent_idx)
{
  if (exists(sent_idx)) { // array at position e.getIndex() already exists
    //enlarge array at position e.getIndex()
    size_t pos = getIndex(sent_idx);
//              TRACE_ERR("Inserting " << e << " in array " << sent_idx << std::endl);
    m_array.at(pos).add(e);
  } else {
//              TRACE_ERR("Creating a new entry in the array and inserting " << e << std::endl);
    FeatureArray a;
    a.NumberOfFeatures(m_num_features);
    a.Features(m_features);
    a.setIndex(sent_idx);
    a.add(e);
    add(a);
  }
}

bool FeatureData::check_consistency() const
{
  if (m_array.size() == 0)
    return true;

  for (featdata_t::const_iterator i = m_array.begin(); i != m_array.end(); i++)
    if (!i->check_consistency()) return false;

  return true;
}

void FeatureData::setIndex()
{
  size_t j=0;
  for (featdata_t::iterator i = m_array.begin(); i !=m_array.end(); i++) {
    m_index_to_array_name[j]=(*i).getIndex();
    m_array_name_to_index[(*i).getIndex()] = j;
    j++;
  }
}

void FeatureData::setFeatureMap(const std::string& feat)
{
  m_num_features = 0;
  m_features = feat;

  vector<string> buf;
  Tokenize(feat.c_str(), ' ', &buf);
  for (vector<string>::const_iterator it = buf.begin();
       it != buf.end(); ++it) {
    const size_t size = m_index_to_feature_name.size();
    m_feature_name_to_index[*it] = size;
    m_index_to_feature_name[size] = *it;
    ++m_num_features;
  }
}

string FeatureData::ToString() const {
  string res;
  char buf[100];

  snprintf(buf, sizeof(buf), "number of features: %lu, ", m_num_features);
  res.append(buf);

  res.append("features: ");
  res.append(m_features);

  snprintf(buf, sizeof(buf), ", sparse flag: %s, ", (m_sparse_flag) ? "yes" : "no");
  res.append(buf);

  res.append("feature_id_map = { ");
  for (map<string, size_t>::const_iterator it = m_feature_name_to_index.begin();
       it != m_feature_name_to_index.end(); ++it) {
    snprintf(buf, sizeof(buf), "%s => %lu, ",
                  it->first.c_str(), it->second);
    res.append(buf);
  }
  res.append("}");

  return res;
}
