// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#ifndef moses_StaticData_h
#define moses_StaticData_h

#include <stdexcept>
#include <limits>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include <fstream>
#include <string>

#ifdef WITH_THREADS
  #ifdef BOOST_HAS_PTHREADS
#include <boost/thread/mutex.hpp>
  #endif
#endif

#include "TypeDef.h"
#include "ScoreIndexManager.h"
#include "FactorCollection.h"
#include "Parameter.h"
#include "LanguageModel.h"
#include "LMList.h"
#include "SentenceStats.h"
#include "DecodeGraph.h"
#include "TranslationOptionList.h"
#include "TranslationSystem.h"
#include "SpecOpt.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif
//#include "UnknownWordHandler.h"

namespace Moses
{

class InputType;
class LexicalReordering;
class GlobalLexicalModel;
class PhraseDictionaryFeature;
class GenerationDictionary;
class DistortionScoreProducer;
class DecodeStep;
class UnknownWordPenaltyProducer;
#ifdef HAVE_SYNLM
class SyntacticLanguageModel;
#endif
class TranslationSystem;

typedef std::pair<std::string, float> UnknownLHSEntry;
typedef std::vector<UnknownLHSEntry>  UnknownLHSList;

// MJD: A function for static initialization of arrays. Used to set
// the container of StaticData pointers at startup.
template<typename T>
T* InitArray(size_t size, T value) {
  T* a = new T[size];
  for(size_t i = 0; i < size; i++)
    a[i] = value;
  return a;
}

/** Contains global variables and contants */
class StaticData
{
private:
  
// MJD: All the stuff for handling multithread dynamic parameters
// with as little locking as possible
#ifdef WITH_THREADS
  #ifdef BOOST_HAS_PTHREADS
    #define MAX_INSTANCES 100
    static boost::mutex s_threadMutex;
    static size_t s_threads_num;
    static pthread_t* s_threads;
    static StaticData** s_instances;
  #endif
#endif

  static StaticData s_instance;
  
  // MJD: A helpful pointer for multithread dynamic parameters
  static StaticData* s_instance_ptr;

protected:

  std::map<long,Phrase> m_constraints;
  std::vector<PhraseDictionaryFeature*>	m_phraseDictionary;
  std::vector<GenerationDictionary*>	m_generationDictionary;
  Parameter *m_parameter;
  std::vector<FactorType>	m_inputFactorOrder, m_outputFactorOrder;
  LMList									m_languageModel;
#ifdef HAVE_SYNLM
	SyntacticLanguageModel* m_syntacticLanguageModel;
#endif
  ScoreIndexManager				m_scoreIndexManager;
  std::vector<float>			m_allWeights;
  std::vector<LexicalReordering*>  m_reorderModels;
  std::vector<GlobalLexicalModel*> m_globalLexicalModels;
  std::vector<DecodeGraph*> m_decodeGraphs;
  std::vector<size_t> m_decodeGraphBackoff;
  // Initial	= 0 = can be used when creating poss trans
  // Other		= 1 = used to calculate LM score once all steps have been processed
  std::map<std::string, TranslationSystem> m_translationSystems;
  float
  m_beamWidth,
  m_earlyDiscardingThreshold,
  m_translationOptionThreshold,
  m_wordDeletionWeight;

  // PhraseTrans, Generation & LanguageModelScore has multiple weights.
  int				m_maxDistortion;
  // do it differently from old pharaoh
  // -ve	= no limit on distortion
  // 0		= no disortion (monotone in old pharaoh)
  bool m_reorderingConstraint; //! use additional reordering constraints
  size_t
  m_maxHypoStackSize //! hypothesis-stack size that triggers pruning
  , m_minHypoStackDiversity //! minimum number of hypothesis in stack for each source word coverage
  , m_nBestSize
  , m_nBestFactor
  , m_maxNoTransOptPerCoverage
  , m_maxNoPartTransOpt
  , m_maxPhraseLength
  , m_numLinkParams;

  std::string
  m_constraintFileName;

  std::string									m_nBestFilePath;
  bool                        m_fLMsLoaded, m_labeledNBestList,m_nBestIncludesAlignment;
  
  // MJD: Parameter for WIPO-specific n-best list format
  bool m_nBestWipo;
  
  // MJD: Compact phrase table and reordering table
  bool m_minphrMemory;
  bool m_minlexrMemory;
  
  bool m_dropUnknown; //! false = treat unknown words as unknowns, and translate them as themselves; true = drop (ignore) them
  bool m_wordDeletionEnabled;

  bool m_disableDiscarding;
  bool m_printAllDerivations;

  bool m_sourceStartPosMattersForRecombination;
  bool m_recoverPath;
  bool m_outputHypoScore;

  SearchAlgorithm m_searchAlgorithm;
  InputTypeEnum m_inputType;
  size_t m_numInputScores;

  mutable size_t m_verboseLevel;
  std::vector<WordPenaltyProducer*> m_wordPenaltyProducers;
  std::vector<DistortionScoreProducer *> m_distortionScoreProducers;
  UnknownWordPenaltyProducer *m_unknownWordPenaltyProducer;
  bool m_reportSegmentation;
  bool m_reportAllFactors;
  bool m_reportAllFactorsNBest;
  std::string m_detailedTranslationReportingFilePath;
  bool m_onlyDistinctNBest;
  bool m_UseAlignmentInfo;
  bool m_PrintAlignmentInfo;
  bool m_PrintAlignmentInfoNbest;

  std::string m_alignmentOutputFile;

  std::string m_factorDelimiter; //! by default, |, but it can be changed
  size_t m_maxFactorIdx[2];  //! number of factors on source and target side
  size_t m_maxNumFactors;  //! max number of factors on both source and target sides

  XmlInputType m_xmlInputType; //! method for handling sentence XML input

  bool m_mbr; //! use MBR decoder
  bool m_useLatticeMBR; //! use MBR decoder
  bool m_useConsensusDecoding; //! Use Consensus decoding  (DeNero et al 2009)
  size_t m_mbrSize; //! number of translation candidates considered
  float m_mbrScale; //! scaling factor for computing marginal probability of candidate translation
  size_t m_lmbrPruning; //! average number of nodes per word wanted in pruned lattice
  std::vector<float> m_lmbrThetas; //! theta(s) for lattice mbr calculation
  bool m_useLatticeHypSetForLatticeMBR; //! to use nbest as hypothesis set during lattice MBR
  float m_lmbrPrecision; //! unigram precision theta - see Tromble et al 08 for more details
  float m_lmbrPRatio; //! decaying factor for ngram thetas - see Tromble et al 08 for more details
  float m_lmbrMapWeight; //! Weight given to the map solution. See Kumar et al 09 for details

  size_t m_lmcache_cleanup_threshold; //! number of translations after which LM claenup is performed (0=never, N=after N translations; default is 1)
  bool m_lmEnableOOVFeature;

  bool m_timeout; //! use timeout
  size_t m_timeout_threshold; //! seconds after which time out is activated

  bool m_useTransOptCache; //! flag indicating, if the persistent translation option cache should be used
  mutable std::map<std::pair<size_t, Phrase>, std::pair<TranslationOptionList*,clock_t> > m_transOptCache; //! persistent translation option cache
  size_t m_transOptCacheMaxSize; //! maximum size for persistent translation option cache
  //FIXME: Single lock for cache not most efficient. However using a
  //reader-writer for LRU cache is tricky - how to record last used time?
#ifdef WITH_THREADS
  mutable boost::mutex m_transOptCacheMutex;
#endif
  bool m_isAlwaysCreateDirectTranslationOption;
  //! constructor. only the 1 static variable can be created

  bool m_outputWordGraph; //! whether to output word graph
  bool m_outputSearchGraph; //! whether to output search graph
  bool m_outputSearchGraphExtended; //! ... in extended format
#ifdef HAVE_PROTOBUF
  bool m_outputSearchGraphPB; //! whether to output search graph as a protobuf
#endif
  bool m_unprunedSearchGraph; //! do not exclude dead ends (chart decoder only)

  size_t m_cubePruningPopLimit;
  size_t m_cubePruningDiversity;
  bool m_cubePruningLazyScoring;
  size_t m_ruleLimit;


  // Initial = 0 = can be used when creating poss trans
  // Other = 1 = used to calculate LM score once all steps have been processed
  Word m_inputDefaultNonTerminal, m_outputDefaultNonTerminal;
  SourceLabelOverlap m_sourceLabelOverlap;
  UnknownLHSList m_unknownLHS;
  WordAlignmentSort m_wordAlignmentSort;

  int m_threadCount;

  StaticData();

  void LoadPhraseBasedParameters();
  void LoadChartDecodingParameters();
  void LoadNonTerminals();

  //! helper fn to set bool param from ini file/command line
  void SetBooleanParameter(bool *paramter, std::string parameterName, bool defaultValue);
  //! load all language models as specified in ini file
  bool LoadLanguageModels();
#ifdef HAVE_SYNLM
  //! load syntactic language model
	bool LoadSyntacticLanguageModel();
#endif
  //! load not only the main phrase table but also any auxiliary tables that depend on which features are being used (e.g., word-deletion, word-insertion tables)
  bool LoadPhraseTables();
  //! load all generation tables as specified in ini file
  bool LoadGenerationTables();
  //! load decoding steps
  bool LoadDecodeGraphs();
  bool LoadLexicalReorderingModel();
  bool LoadGlobalLexicalModel();
  void ReduceTransOptCache() const;
  bool m_continuePartialTranslation;

public:
  // MJD: Added copy constructor. See StaticData.cpp
  StaticData(const StaticData& s);

  bool IsAlwaysCreateDirectTranslationOption() const {
    return m_isAlwaysCreateDirectTranslationOption;
  }
  //! destructor
  ~StaticData();
  
  #ifdef WITH_THREADS
  #ifdef BOOST_HAS_PTHREADS
  
  // MJD: has to be called once by each thread before translating a sentence
  // with dynamic paramters. See e.g. moses-cmd/src/Main.cpp.
  // Locking is only neccessary if the thread is assigned for the first time.
  // After that, each thread will access only its own slot. 
  static void CreateThreadInstance() {
    pthread_t thisThread = pthread_self();
    for(size_t i = 0; i < s_threads_num; i++) {
      if(s_threads[i] == thisThread) {
	s_instances[i] = new StaticData(s_instance);
	return;
      }
    }
    if(s_threads_num < MAX_INSTANCES) {
      boost::mutex::scoped_lock lock(s_threadMutex);
      if(s_threads[s_threads_num] == 0) {
	s_threads[s_threads_num] = thisThread;
	s_instances[s_threads_num] = new StaticData(s_instance);
	s_threads_num++;
	std::cerr << "Registered thread id" << thisThread << " " << s_threads_num << std::endl;
	return;
      }
    }
  }
  
  // MJD: has to be called after each sentence to destroy the modified StaticData
  // instance. Counterpart to the previous function. No locking is needed for the
  // same reseasons as above. Only StaticData objects that are copies are destroyed.
  // The main StaticData instance remains untouched. 
  static void DeleteThreadInstance() {
    pthread_t thisThread = pthread_self();
    for(size_t i = 0; i < s_threads_num; i++) {
      if(s_threads[i] == thisThread) {
	if(s_instances[i] != &s_instance)
	  delete s_instances[i];
	s_instances[i] = &s_instance;
	return;
      }
    }
  }
  #endif
  #endif
  
  // MJD: Dynamic parameter handling with single thread.  
  static void CreateTempInstance() {
    s_instance_ptr = new StaticData(s_instance);
  }
  
  // MJD: Dynamic parameter handling with single thread.  
  static void DeleteTempInstance() {
    if(s_instance_ptr != &s_instance) {
      StaticData* t_ptr = s_instance_ptr;
      s_instance_ptr = &s_instance;
      delete t_ptr;
    }
  }
    
  //! return static instance for use like global variable
  static const StaticData& Instance() {
    // MJD: return the StaticData instance assigned to this thread 
    #ifdef WITH_THREADS
      #ifdef BOOST_HAS_PTHREADS
      pthread_t thisThread = pthread_self();
      for(size_t i = 0; i < s_threads_num; i++)
	if(s_threads[i] == thisThread)
	  return *s_instances[i];
      #endif
    #endif
    
    return *s_instance_ptr;
  }

  /** delete current static instance and replace with another.
  	* Used by gui front end
  	*/
#ifdef WIN32
  static void Reset() {
    // MJD: Probably should do something with that, but who uses windows?
    s_instance = StaticData();
  }
#endif

  //! Load data into static instance. This function is required as LoadData() is not const
  static bool LoadDataStatic(Parameter *parameter) {
    return s_instance.LoadData(parameter);
  }

  //! Main function to load everything. Also initialize the Parameter object
  bool LoadData(Parameter *parameter);

  const PARAM_VEC &GetParam(const std::string &paramName) const {
    return m_parameter->GetParam(paramName);
  }

  const std::vector<FactorType> &GetInputFactorOrder() const {
    return m_inputFactorOrder;
  }
  const std::vector<FactorType> &GetOutputFactorOrder() const {
    return m_outputFactorOrder;
  }


  inline bool GetSourceStartPosMattersForRecombination() const {
    return m_sourceStartPosMattersForRecombination;
  }
  inline bool GetDropUnknown() const {
    return m_dropUnknown;
  }
  inline bool GetDisableDiscarding() const {
    return m_disableDiscarding;
  }
  inline size_t GetMaxNoTransOptPerCoverage() const {
    return m_maxNoTransOptPerCoverage;
  }
  inline size_t GetMaxNoPartTransOpt() const {
    return m_maxNoPartTransOpt;
  }
  inline const Phrase* GetConstrainingPhrase(long sentenceID) const {
    std::map<long,Phrase>::const_iterator iter = m_constraints.find(sentenceID);
    if (iter != m_constraints.end()) {
      const Phrase& phrase = iter->second;
      return &phrase;
    } else {
      return NULL;
    }
  }
  inline size_t GetMaxPhraseLength() const {
    return m_maxPhraseLength;
  }
  bool IsWordDeletionEnabled() const {
    return m_wordDeletionEnabled;
  }
  size_t GetMaxHypoStackSize() const {
    return m_maxHypoStackSize;
  }
  size_t GetMinHypoStackDiversity() const {
    return m_minHypoStackDiversity;
  }
  size_t GetCubePruningPopLimit() const {
    return m_cubePruningPopLimit;
  }
  size_t GetCubePruningDiversity() const {
    return m_cubePruningDiversity;
  }
  bool GetCubePruningLazyScoring() const {
    return m_cubePruningLazyScoring;
  }
  size_t IsPathRecoveryEnabled() const {
    return m_recoverPath;
  }
  int GetMaxDistortion() const {
    return m_maxDistortion;
  }
  bool UseReorderingConstraint() const {
    return m_reorderingConstraint;
  }
  float GetBeamWidth() const {
    return m_beamWidth;
  }
  float GetEarlyDiscardingThreshold() const {
    return m_earlyDiscardingThreshold;
  }
  bool UseEarlyDiscarding() const {
    return m_earlyDiscardingThreshold != -std::numeric_limits<float>::infinity();
  }
  float GetTranslationOptionThreshold() const {
    return m_translationOptionThreshold;
  }
  //! returns the total number of score components across all types, all factors
  size_t GetTotalScoreComponents() const {
    return m_scoreIndexManager.GetTotalNumberOfScores();
  }
  const ScoreIndexManager& GetScoreIndexManager() const {
    return m_scoreIndexManager;
  }

  const TranslationSystem& GetTranslationSystem(std::string id) const {
    std::map<std::string, TranslationSystem>::const_iterator iter =
      m_translationSystems.find(id);
    if (iter == m_translationSystems.end()) {
      VERBOSE(1, "Translation system not found " << id << std::endl);
      throw std::runtime_error("Unknown translation system id");
    } else {
      return iter->second;
    }
  }
  size_t GetVerboseLevel() const {
    return m_verboseLevel;
  }
  void SetVerboseLevel(int x) const {
    m_verboseLevel = x;
  }
  bool GetReportSegmentation() const {
    return m_reportSegmentation;
  }
  bool GetReportAllFactors() const {
    return m_reportAllFactors;
  }
  bool GetReportAllFactorsNBest() const {
    return m_reportAllFactorsNBest;
  }
  bool IsDetailedTranslationReportingEnabled() const {
    return !m_detailedTranslationReportingFilePath.empty();
  }
  const std::string &GetDetailedTranslationReportingFilePath() const {
    return m_detailedTranslationReportingFilePath;
  }

  const std::string &GetAlignmentOutputFile() const {
    return m_alignmentOutputFile;
  }

  bool IsLabeledNBestList() const {
    return m_labeledNBestList;
  }
  bool NBestIncludesAlignment() const {
    return m_nBestIncludesAlignment;
  }
  
  // MJD: WIPO-specific n-best list
  bool UseNBestWipoFormat() const {
     return m_nBestWipo;
  }

  // MJD:
  bool UseMinphrInMemory() const {
     return m_minphrMemory;
  }

  // MJD: 
  bool UseMinlexrInMemory() const {
     return m_minlexrMemory;
  }
  
  size_t GetNumLinkParams() const {
    return m_numLinkParams;
  }
  const std::vector<std::string> &GetDescription() const {
    return m_parameter->GetParam("description");
  }

  // for mert
  size_t GetNBestSize() const {
    return m_nBestSize;
  }
  const std::string &GetNBestFilePath() const {
    return m_nBestFilePath;
  }
  bool IsNBestEnabled() const {
    return (!m_nBestFilePath.empty()) || m_mbr || m_useLatticeMBR || m_outputSearchGraph || m_useConsensusDecoding
#ifdef HAVE_PROTOBUF
           || m_outputSearchGraphPB
#endif
           ;
  }
  size_t GetNBestFactor() const {
    return m_nBestFactor;
  }
  bool GetOutputWordGraph() const {
    return m_outputWordGraph;
  }

  //! Sets the global score vector weights for a given ScoreProducer.
  void SetWeightsForScoreProducer(const ScoreProducer* sp, const std::vector<float>& weights);
  InputTypeEnum GetInputType() const {
    return m_inputType;
  }
  SearchAlgorithm GetSearchAlgorithm() const {
    return m_searchAlgorithm;
  }
  LMList GetLMList() const { 
    return m_languageModel; 
  }
  size_t GetNumInputScores() const {
    return m_numInputScores;
  }

  const std::vector<float>& GetAllWeights() const {
    return m_allWeights;
  }

  bool UseAlignmentInfo() const {
    return m_UseAlignmentInfo;
  }
  void UseAlignmentInfo(bool a) {
    m_UseAlignmentInfo=a;
  };
  bool PrintAlignmentInfo() const {
    return m_PrintAlignmentInfo;
  }
  bool PrintAlignmentInfoInNbest() const {
    return m_PrintAlignmentInfoNbest;
  }
  bool GetDistinctNBest() const {
    return m_onlyDistinctNBest;
  }
  const std::string& GetFactorDelimiter() const {
    return m_factorDelimiter;
  }
  size_t GetMaxNumFactors(FactorDirection direction) const {
    return m_maxFactorIdx[(size_t)direction]+1;
  }
  size_t GetMaxNumFactors() const {
    return m_maxNumFactors;
  }
  bool UseMBR() const {
    return m_mbr;
  }
  bool UseLatticeMBR() const {
    return m_useLatticeMBR ;
  }
  bool UseConsensusDecoding() const {
    return m_useConsensusDecoding;
  }
  void SetUseLatticeMBR(bool flag) {
    m_useLatticeMBR = flag;
  }
  size_t GetMBRSize() const {
    return m_mbrSize;
  }
  float GetMBRScale() const {
    return m_mbrScale;
  }
  void SetMBRScale(float scale) {
    m_mbrScale = scale;
  }
  size_t GetLatticeMBRPruningFactor() const {
    return m_lmbrPruning;
  }
  void SetLatticeMBRPruningFactor(size_t prune) {
    m_lmbrPruning = prune;
  }
  const std::vector<float>& GetLatticeMBRThetas() const {
    return m_lmbrThetas;
  }
  bool  UseLatticeHypSetForLatticeMBR() const {
    return m_useLatticeHypSetForLatticeMBR;
  }
  float GetLatticeMBRPrecision() const {
    return m_lmbrPrecision;
  }
  void SetLatticeMBRPrecision(float p) {
    m_lmbrPrecision = p;
  }
  float GetLatticeMBRPRatio() const {
    return m_lmbrPRatio;
  }
  void SetLatticeMBRPRatio(float r) {
    m_lmbrPRatio = r;
  }

  float GetLatticeMBRMapWeight() const {
    return m_lmbrMapWeight;
  }

  bool UseTimeout() const {
    return m_timeout;
  }
  size_t GetTimeoutThreshold() const {
    return m_timeout_threshold;
  }

  size_t GetLMCacheCleanupThreshold() const {
    return m_lmcache_cleanup_threshold;
  }

  bool GetLMEnableOOVFeature() const {
    return m_lmEnableOOVFeature;
  }

  bool GetOutputSearchGraph() const {
    return m_outputSearchGraph;
  }
  void SetOutputSearchGraph(bool outputSearchGraph) {
    m_outputSearchGraph = outputSearchGraph;
  }
  bool GetOutputSearchGraphExtended() const {
    return m_outputSearchGraphExtended;
  }
#ifdef HAVE_PROTOBUF
  bool GetOutputSearchGraphPB() const {
    return m_outputSearchGraphPB;
  }
#endif
  bool GetUnprunedSearchGraph() const {
    return m_unprunedSearchGraph;
  }

  XmlInputType GetXmlInputType() const {
    return m_xmlInputType;
  }

  bool GetUseTransOptCache() const {
    return m_useTransOptCache;
  }

  void AddTransOptListToCache(const DecodeGraph &decodeGraph, const Phrase &sourcePhrase, const TranslationOptionList &transOptList) const;

  void ClearTransOptionCache() const;


  const TranslationOptionList* FindTransOptListInCache(const DecodeGraph &decodeGraph, const Phrase &sourcePhrase) const;

  bool PrintAllDerivations() const {
    return m_printAllDerivations;
  }

  const UnknownLHSList &GetUnknownLHS() const {
    return m_unknownLHS;
  }

  const Word &GetInputDefaultNonTerminal() const {
    return m_inputDefaultNonTerminal;
  }
  const Word &GetOutputDefaultNonTerminal() const {
    return m_outputDefaultNonTerminal;
  }

  SourceLabelOverlap GetSourceLabelOverlap() const {
    return m_sourceLabelOverlap;
  }

  bool GetOutputHypoScore() const {
    return m_outputHypoScore;
  }
  size_t GetRuleLimit() const {
    return m_ruleLimit;
  }
  float GetRuleCountThreshold() const {
    return 999999; /* TODO wtf! */
  }

  bool ContinuePartialTranslation() const {
    return m_continuePartialTranslation;
  }

  WordAlignmentSort GetWordAlignmentSort() const {
    return m_wordAlignmentSort;
  }

  int ThreadCount() const {
    return m_threadCount;
  }
  
  // MJD: Sets a named translation system parameter, returns the previous value.
  const WeightInfos SetTranslationSystemWeights(const TranslationSystem& system, const WeightInfos& newWeights);
  WeightInfo SetTranslationSystemWeight(const TranslationSystem& system, WeightInfo wi);
  
  // MJD: Sets a collection of global parameters, returns the previous set of parameters.
  Parameter SetGlobalParameters(const Parameter&);
};

}
#endif
