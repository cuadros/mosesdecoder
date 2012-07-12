// $Id$
// vim:tabstop=2

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

#include <string>
#include <cassert>
#include "PhraseDictionaryMemory.h"
#include "DecodeStepTranslation.h"
#include "DecodeStepGeneration.h"
#include "GenerationDictionary.h"
#include "DummyScoreProducers.h"
#include "StaticData.h"
#include "Util.h"
#include "FactorCollection.h"
#include "Timer.h"
#include "LanguageModelFactory.h"
#include "LexicalReordering.h"
#include "GlobalLexicalModel.h"
#include "SentenceStats.h"
#include "PhraseDictionary.h"
#include "UserMessage.h"
#include "TranslationOption.h"
#include "DecodeGraph.h"
#include "InputFileStream.h"

#ifdef HAVE_SYNLM
#include "SyntacticLanguageModel.h"
#endif

#ifdef WITH_THREADS
#include <boost/thread.hpp>
#endif

using namespace std;

namespace Moses
{
static size_t CalcMax(size_t x, const vector<size_t>& y)
{
  size_t max = x;
  for (vector<size_t>::const_iterator i=y.begin(); i != y.end(); ++i)
    if (*i > max) max = *i;
  return max;
}

static size_t CalcMax(size_t x, const vector<size_t>& y, const vector<size_t>& z)
{
  size_t max = x;
  for (vector<size_t>::const_iterator i=y.begin(); i != y.end(); ++i)
    if (*i > max) max = *i;
  for (vector<size_t>::const_iterator i=z.begin(); i != z.end(); ++i)
    if (*i > max) max = *i;
  return max;
}

StaticData StaticData::s_instance;

// MJD: This pointer is used to substitute StaticData instances
// with multi-thread and single-thread compilation
StaticData* StaticData::s_instance_ptr = &StaticData::s_instance;

// MJD: TODO: Write something meaningful here!
#ifdef WITH_THREADS
#ifdef BOOST_HAS_PTHREADS
boost::mutex StaticData::s_threadMutex;
size_t StaticData::s_threads_num = 0;

pthread_t* StaticData::s_threads = InitArray(MAX_INSTANCES, pthread_t(0));
StaticData** StaticData::s_instances = InitArray(MAX_INSTANCES, &StaticData::s_instance);
#endif
#endif

StaticData::StaticData()
  :m_numLinkParams(1)
  ,m_fLMsLoaded(false)
  ,m_sourceStartPosMattersForRecombination(false)
  ,m_inputType(SentenceInput)
  ,m_numInputScores(0)
  ,m_detailedTranslationReportingFilePath()
  ,m_onlyDistinctNBest(false)
  ,m_lmEnableOOVFeature(false)
  ,m_factorDelimiter("|") // default delimiter between factors
  ,m_isAlwaysCreateDirectTranslationOption(false)
{
  m_maxFactorIdx[0] = 0;  // source side
  m_maxFactorIdx[1] = 0;  // target side

  // memory pools
  Phrase::InitializeMemPool();
}

// MJD: Huge copy constructor for StaticData. Performs all necessary shallow
// and not so shallow copying of all the scoreproducers, translation systems
// and everything else that was needed. TODO: Add comments inside the function.

StaticData::StaticData(const StaticData& s)
 : m_numLinkParams(s.m_numLinkParams),
m_sourceStartPosMattersForRecombination(s.m_sourceStartPosMattersForRecombination),
m_fLMsLoaded(s.m_fLMsLoaded), m_inputType(s.m_inputType), m_numInputScores(s.m_numInputScores),
m_detailedTranslationReportingFilePath(s.m_detailedTranslationReportingFilePath),
m_onlyDistinctNBest(s.m_onlyDistinctNBest), m_factorDelimiter(s.m_factorDelimiter),
m_isAlwaysCreateDirectTranslationOption(s.m_isAlwaysCreateDirectTranslationOption),
m_constraints(s.m_constraints), 
m_inputFactorOrder(s.m_inputFactorOrder), m_outputFactorOrder(s.m_outputFactorOrder),
m_allWeights(s.m_allWeights), m_beamWidth(s.m_beamWidth),
m_earlyDiscardingThreshold(s.m_earlyDiscardingThreshold),
m_translationOptionThreshold(s.m_translationOptionThreshold),
m_wordDeletionWeight(s.m_wordDeletionWeight),
m_maxDistortion(s.m_maxDistortion), m_reorderingConstraint(s.m_reorderingConstraint),
m_maxHypoStackSize(s.m_maxHypoStackSize), m_minHypoStackDiversity(s.m_minHypoStackDiversity),
m_nBestSize(s.m_nBestSize), m_nBestFactor(s.m_nBestFactor),
m_maxNoTransOptPerCoverage(s.m_maxNoTransOptPerCoverage), m_maxNoPartTransOpt(s.m_maxNoPartTransOpt),
m_maxPhraseLength(s.m_maxPhraseLength),
m_constraintFileName(s.m_constraintFileName), m_nBestFilePath(s.m_nBestFilePath),
m_dropUnknown(s.m_dropUnknown), m_wordDeletionEnabled(s.m_wordDeletionEnabled),
m_disableDiscarding(s.m_disableDiscarding), m_printAllDerivations(s.m_printAllDerivations),
m_recoverPath(s.m_recoverPath), m_outputHypoScore(s.m_outputHypoScore), m_searchAlgorithm(s.m_searchAlgorithm),
m_verboseLevel(s.m_verboseLevel), m_reportSegmentation(s.m_reportSegmentation),
m_reportAllFactors(s.m_reportAllFactors), m_reportAllFactorsNBest(s.m_reportAllFactorsNBest), 
m_UseAlignmentInfo(s.m_UseAlignmentInfo), m_PrintAlignmentInfo(s.m_PrintAlignmentInfo),
m_PrintAlignmentInfoNbest(s.m_PrintAlignmentInfoNbest),
m_alignmentOutputFile(s.m_alignmentOutputFile), m_maxNumFactors(s.m_maxNumFactors),
m_xmlInputType(s.m_xmlInputType), m_mbr(s.m_mbr),
m_useLatticeMBR(s.m_useLatticeMBR), m_useConsensusDecoding(s.m_useConsensusDecoding),
m_mbrSize(s.m_mbrSize), m_mbrScale(s.m_mbrScale), m_lmbrPruning(s.m_lmbrPruning),
m_lmbrThetas(s.m_lmbrThetas), m_useLatticeHypSetForLatticeMBR(s.m_useLatticeHypSetForLatticeMBR),
m_lmbrPrecision(s.m_lmbrPrecision), m_lmbrPRatio(s.m_lmbrPRatio), m_lmbrMapWeight(s.m_lmbrMapWeight),
m_lmcache_cleanup_threshold(s.m_lmcache_cleanup_threshold), m_timeout(s.m_timeout),
m_timeout_threshold(s.m_timeout_threshold), m_useTransOptCache(s.m_useTransOptCache),
m_transOptCacheMaxSize(s.m_transOptCacheMaxSize),
m_outputWordGraph(s.m_outputWordGraph), m_outputSearchGraph(s.m_outputSearchGraph),
m_outputSearchGraphExtended(s.m_outputSearchGraphExtended),
#ifdef HAVE_PROTOBUF
m_outputSearchGraphPB(s.m_outputSearchGraphPB),
#endif
m_cubePruningPopLimit(s.m_cubePruningPopLimit), m_cubePruningDiversity(s.m_cubePruningDiversity),
m_ruleLimit(s.m_ruleLimit), m_inputDefaultNonTerminal(s.m_inputDefaultNonTerminal),
m_sourceLabelOverlap(s.m_sourceLabelOverlap), m_unknownLHS(s.m_unknownLHS),
m_labeledNBestList(s.m_labeledNBestList), m_nBestIncludesAlignment(s.m_nBestIncludesAlignment),
m_nBestWipo(s.m_nBestWipo), m_minphrMemory(s.m_minphrMemory), m_minlexrMemory(s.m_minlexrMemory)
{ 
  m_parameter = new Parameter(*(s.m_parameter));
  
  for(std::vector<DistortionScoreProducer*>::const_iterator it = s.m_distortionScoreProducers.begin(); it != s.m_distortionScoreProducers.end(); it++) {
    DistortionScoreProducer* dsp = new DistortionScoreProducer(**it);
    m_distortionScoreProducers.push_back(dsp);
    m_scoreIndexManager.AddScoreProducer(dsp, true);
  }

  for(std::vector<WordPenaltyProducer*>::const_iterator it = s.m_wordPenaltyProducers.begin(); it != s.m_wordPenaltyProducers.end(); it++) {
    WordPenaltyProducer* wpp = new WordPenaltyProducer(**it);
    m_wordPenaltyProducers.push_back(wpp);
    m_scoreIndexManager.AddScoreProducer(wpp, true);
  }
  
  m_unknownWordPenaltyProducer = new UnknownWordPenaltyProducer(*(s.m_unknownWordPenaltyProducer));
  m_scoreIndexManager.AddScoreProducer(m_unknownWordPenaltyProducer, true);
  
  for(std::vector<LexicalReordering*>::const_iterator it = s.m_reorderModels.begin(); it != s.m_reorderModels.end(); it++) {
    LexicalReordering* lrm = new LexicalReordering(**it);
    m_reorderModels.push_back(lrm);
    m_scoreIndexManager.AddScoreProducer(lrm, true);
  }
  
  for (LMList::const_iterator lmIt = s.m_languageModel.begin(); lmIt != s.m_languageModel.end(); ++lmIt) {
    LanguageModel* lm = new LanguageModel(**lmIt);
    m_scoreIndexManager.AddScoreProducer(lm, true);
    m_languageModel.Add(lm);
  }

  #ifdef HAVE_SYNLM
  m_syntacticLanguageModel = new SyntacticLanguageModel(*(s.m_syntacticLanguageModel));
  #endif

  for(std::vector<GenerationDictionary*>::const_iterator it = s.m_generationDictionary.begin(); it != s.m_generationDictionary.end(); it++) {
    GenerationDictionary* gen = new GenerationDictionary(**it);
    m_generationDictionary.push_back(gen);
    m_scoreIndexManager.AddScoreProducer(gen, true);
  }
  
  for(std::vector<PhraseDictionaryFeature*>::const_iterator it = s.m_phraseDictionary.begin(); it != s.m_phraseDictionary.end(); it++) {
    PhraseDictionaryFeature* pdf = new PhraseDictionaryFeature(**it);
    m_phraseDictionary.push_back(pdf);
    m_scoreIndexManager.AddScoreProducer(pdf, true);
  }
   
  for (size_t i = 0; i < s.m_decodeGraphs.size(); ++i) {
    DecodeGraph* oldDecodeGraph = s.m_decodeGraphs[i];
    DecodeGraph* newDecodeGraph = new DecodeGraph(oldDecodeGraph->GetPosition());
    
    const DecodeStep* prev = 0;
    for (DecodeGraph::const_iterator j = oldDecodeGraph->begin(); j != oldDecodeGraph->end(); ++j) {
      const DecodeStep* step = *j;
	
      if(step->GetPhraseDictionaryFeature())
        for(std::vector<PhraseDictionaryFeature*>::const_iterator it = m_phraseDictionary.begin(); it != m_phraseDictionary.end(); it++)
          if(step->GetPhraseDictionaryFeature()->GetScoreBookkeepingID() == (*it)->GetScoreBookkeepingID()) {
            DecodeStep* newDecodeStep = new DecodeStepTranslation(*it, prev);
            newDecodeGraph->Add(newDecodeStep);
          }
     
      if(step->GetGenerationDictionaryFeature())
        for(std::vector<GenerationDictionary*>::const_iterator it = m_generationDictionary.begin(); it != m_generationDictionary.end(); it++)
          if(step->GetGenerationDictionaryFeature()->GetScoreBookkeepingID() == (*it)->GetScoreBookkeepingID()) {
            DecodeStep* newDecodeStep = new DecodeStepGeneration(*it, prev);
            newDecodeGraph->Add(newDecodeStep);
          }
     
      prev = step;
    }
    m_decodeGraphs.push_back(newDecodeGraph);
    m_decodeGraphBackoff.push_back(s.m_decodeGraphBackoff[i]);
  }
  
  m_scoreIndexManager.InitFeatureNames();
  
  for(std::map<std::string, TranslationSystem>::const_iterator it = s.m_translationSystems.begin(); it != s.m_translationSystems.end(); it++) {
    const TranslationSystem& oldSystem = it->second;
    std::string systemId = oldSystem.GetId();
    
    DistortionScoreProducer* dsc;
    for(std::vector<DistortionScoreProducer*>::const_iterator it = m_distortionScoreProducers.begin(); it != m_distortionScoreProducers.end(); it++)
      if(oldSystem.GetDistortionProducer()->GetScoreBookkeepingID() == (*it)->GetScoreBookkeepingID())
        dsc = *it;
    
    WordPenaltyProducer* wpp;
    for(std::vector<WordPenaltyProducer*>::const_iterator it = m_wordPenaltyProducers.begin(); it != m_wordPenaltyProducers.end(); it++)
      if(oldSystem.GetWordPenaltyProducer()->GetScoreBookkeepingID() == (*it)->GetScoreBookkeepingID())
        wpp = *it;
        
    if(dsc && wpp)
      m_translationSystems.insert(
        std::pair<std::string, TranslationSystem>(
          systemId, TranslationSystem(systemId, wpp, m_unknownWordPenaltyProducer, dsc)));
      
    if(m_translationSystems.count(systemId)) {
      TranslationSystem& system = const_cast<TranslationSystem&>(GetTranslationSystem(systemId));
      
      for(size_t i = 0; i < s.m_decodeGraphs.size(); i++)
        for(size_t j = 0; j < oldSystem.GetDecodeGraphs().size(); j++)
          if(s.m_decodeGraphs[i] == oldSystem.GetDecodeGraphs()[j])
            system.AddDecodeGraph(m_decodeGraphs[i], m_decodeGraphBackoff[i]);
      
      for(size_t i = 0; i < s.m_reorderModels.size(); i++)
        for(size_t j = 0; j < oldSystem.GetReorderModels().size(); j++)
          if(s.m_reorderModels[i] == oldSystem.GetReorderModels()[j])
            system.AddReorderModel(m_reorderModels[i]);
            
      for(size_t i = 0; i < s.m_globalLexicalModels.size(); i++)
        for(size_t j = 0; j < oldSystem.GetGlobalLexicalModels().size(); j++)
          if(s.m_globalLexicalModels[i] == oldSystem.GetGlobalLexicalModels()[j])
            system.AddGlobalLexicalModel(m_globalLexicalModels[i]);
      
      for(LMList::const_iterator i = s.m_languageModel.begin(); i != s.m_languageModel.end(); i++)
        for(LMList::const_iterator j = oldSystem.GetLanguageModels().begin(); j != oldSystem.GetLanguageModels().end(); j++)
          if(*i == *j) {
            size_t d = std::distance(s.m_languageModel.begin(), i);
            LMList::const_iterator k = m_languageModel.begin();
            while(d-- > 0)
              k++;
            system.AddLanguageModel(*k);
          }
      
      system.ConfigDictionaries();
      
      #ifdef HAVE_SYNLM
      
      if (m_syntacticLanguageModel != NULL)
        system.AddFeatureFunction(m_syntacticLanguageModel);
      #endif
    }
  }
  
  m_maxFactorIdx[0] = s.m_maxFactorIdx[0];
  m_maxFactorIdx[1] = s.m_maxFactorIdx[1];
}

bool StaticData::LoadData(Parameter *parameter)
{
  ResetUserTime();
  m_parameter = parameter;

  // verbose level
  m_verboseLevel = 1;
  if (m_parameter->GetParam("verbose").size() == 1) {
    m_verboseLevel = Scan<size_t>( m_parameter->GetParam("verbose")[0]);
  }
  
  // MJD: Parse the file given with this switch and initialize the given named
  // parameter sets. They are now available as long as the process runs.
  if (m_parameter->GetParam("wipo-specopt-file").size() == 1) {
    std::string wipoSpecOptFilename = m_parameter->GetParam("wipo-specopt-file")[0];
    SpecOpt::InitializeFromFile(wipoSpecOptFilename);
  }

  // to cube or not to cube
  m_searchAlgorithm = (m_parameter->GetParam("search-algorithm").size() > 0) ?
                      (SearchAlgorithm) Scan<size_t>(m_parameter->GetParam("search-algorithm")[0]) : Normal;

  if (m_searchAlgorithm == ChartDecoding)
    LoadChartDecodingParameters();
  else
    LoadPhraseBasedParameters();

  // input type has to be specified BEFORE loading the phrase tables!
  if(m_parameter->GetParam("inputtype").size())
    m_inputType= (InputTypeEnum) Scan<int>(m_parameter->GetParam("inputtype")[0]);
  std::string s_it = "text input";
  if (m_inputType == 1) {
    s_it = "confusion net";
  }
  if (m_inputType == 2) {
    s_it = "word lattice";
  }
  VERBOSE(2,"input type is: "<<s_it<<"\n");

  if(m_parameter->GetParam("recover-input-path").size()) {
    m_recoverPath = Scan<bool>(m_parameter->GetParam("recover-input-path")[0]);
    if (m_recoverPath && m_inputType == SentenceInput) {
      TRACE_ERR("--recover-input-path should only be used with confusion net or word lattice input!\n");
      m_recoverPath = false;
    }
  }

  if(m_parameter->GetParam("sort-word-alignment").size()) {
    m_wordAlignmentSort = (WordAlignmentSort) Scan<size_t>(m_parameter->GetParam("sort-word-alignment")[0]);
  }
  
  // factor delimiter
  if (m_parameter->GetParam("factor-delimiter").size() > 0) {
    m_factorDelimiter = m_parameter->GetParam("factor-delimiter")[0];
  }

  SetBooleanParameter( &m_continuePartialTranslation, "continue-partial-translation", false );

  //word-to-word alignment
  SetBooleanParameter( &m_UseAlignmentInfo, "use-alignment-info", false );
  SetBooleanParameter( &m_PrintAlignmentInfo, "print-alignment-info", false );
  SetBooleanParameter( &m_PrintAlignmentInfoNbest, "print-alignment-info-in-n-best", false );
  // MJD: Activate WIPO-specific n-best list format, probably useless for anyone else.
  SetBooleanParameter( &m_nBestWipo, "wipo-n-best", false );
  
  // MJD: compact phrase table and reordering model
  SetBooleanParameter( &m_minphrMemory, "minphr-memory", false );
  SetBooleanParameter( &m_minlexrMemory, "minlexr-memory", false );
  
  SetBooleanParameter( &m_outputHypoScore, "output-hypo-score", false );

  if (!m_UseAlignmentInfo && m_PrintAlignmentInfo) {
    TRACE_ERR("--print-alignment-info should only be used together with \"--use-alignment-info true\". Continue forcing to false.\n");
    m_PrintAlignmentInfo=false;
  }
  if (!m_UseAlignmentInfo && m_PrintAlignmentInfoNbest) {
    TRACE_ERR("--print-alignment-info-in-n-best should only be used together with \"--use-alignment-info true\". Continue forcing to false.\n");
    m_PrintAlignmentInfoNbest=false;
  }

  if (m_parameter->GetParam("alignment-output-file").size() > 0) {
    m_alignmentOutputFile = Scan<std::string>(m_parameter->GetParam("alignment-output-file")[0]);
  }

  // n-best
  if (m_parameter->GetParam("n-best-list").size() >= 2) {
    m_nBestFilePath = m_parameter->GetParam("n-best-list")[0];
    m_nBestSize = Scan<size_t>( m_parameter->GetParam("n-best-list")[1] );
    m_onlyDistinctNBest=(m_parameter->GetParam("n-best-list").size()>2 && m_parameter->GetParam("n-best-list")[2]=="distinct");
  } else if (m_parameter->GetParam("n-best-list").size() == 1) {
    UserMessage::Add(string("ERROR: wrong format for switch -n-best-list file size"));
    return false;
  } else {
    m_nBestSize = 0;
  }
  if (m_parameter->GetParam("n-best-factor").size() > 0) {
    m_nBestFactor = Scan<size_t>( m_parameter->GetParam("n-best-factor")[0]);
  } else {
    m_nBestFactor = 20;
  }

  // word graph
  if (m_parameter->GetParam("output-word-graph").size() == 2)
    m_outputWordGraph = true;
  else
    m_outputWordGraph = false;

  // search graph
  if (m_parameter->GetParam("output-search-graph").size() > 0) {
    if (m_parameter->GetParam("output-search-graph").size() != 1) {
      UserMessage::Add(string("ERROR: wrong format for switch -output-search-graph file"));
      return false;
    }
    m_outputSearchGraph = true;
  }
  // ... in extended format
  else if (m_parameter->GetParam("output-search-graph-extended").size() > 0) {
    if (m_parameter->GetParam("output-search-graph-extended").size() != 1) {
      UserMessage::Add(string("ERROR: wrong format for switch -output-search-graph-extended file"));
      return false;
    }
    m_outputSearchGraph = true;
    m_outputSearchGraphExtended = true;
  } else
    m_outputSearchGraph = false;
#ifdef HAVE_PROTOBUF
  if (m_parameter->GetParam("output-search-graph-pb").size() > 0) {
    if (m_parameter->GetParam("output-search-graph-pb").size() != 1) {
      UserMessage::Add(string("ERROR: wrong format for switch -output-search-graph-pb path"));
      return false;
    }
    m_outputSearchGraphPB = true;
  } else
    m_outputSearchGraphPB = false;
#endif
  SetBooleanParameter( &m_unprunedSearchGraph, "unpruned-search-graph", true );

  // include feature names in the n-best list
  SetBooleanParameter( &m_labeledNBestList, "labeled-n-best-list", true );

  // include word alignment in the n-best list
  SetBooleanParameter( &m_nBestIncludesAlignment, "include-alignment-in-n-best", false );

  // printing source phrase spans
  SetBooleanParameter( &m_reportSegmentation, "report-segmentation", false );

  // print all factors of output translations
  SetBooleanParameter( &m_reportAllFactors, "report-all-factors", false );

  // print all factors of output translations
  SetBooleanParameter( &m_reportAllFactorsNBest, "report-all-factors-in-n-best", false );

  //
  if (m_inputType == SentenceInput) {
    SetBooleanParameter( &m_useTransOptCache, "use-persistent-cache", true );
    m_transOptCacheMaxSize = (m_parameter->GetParam("persistent-cache-size").size() > 0)
                             ? Scan<size_t>(m_parameter->GetParam("persistent-cache-size")[0]) : DEFAULT_MAX_TRANS_OPT_CACHE_SIZE;
  } else {
    m_useTransOptCache = false;
  }


  //input factors
  const vector<string> &inputFactorVector = m_parameter->GetParam("input-factors");
  for(size_t i=0; i<inputFactorVector.size(); i++) {
    m_inputFactorOrder.push_back(Scan<FactorType>(inputFactorVector[i]));
  }
  if(m_inputFactorOrder.empty()) {
    UserMessage::Add(string("no input factor specified in config file"));
    return false;
  }

  //output factors
  const vector<string> &outputFactorVector = m_parameter->GetParam("output-factors");
  for(size_t i=0; i<outputFactorVector.size(); i++) {
    m_outputFactorOrder.push_back(Scan<FactorType>(outputFactorVector[i]));
  }
  if(m_outputFactorOrder.empty()) {
    // default. output factor 0
    m_outputFactorOrder.push_back(0);
  }

  //source word deletion
  SetBooleanParameter( &m_wordDeletionEnabled, "phrase-drop-allowed", false );

  //Disable discarding
  SetBooleanParameter(&m_disableDiscarding, "disable-discarding", false);

  //Print All Derivations
  SetBooleanParameter( &m_printAllDerivations , "print-all-derivations", false );

  // additional output
  if (m_parameter->isParamSpecified("translation-details")) {
    const vector<string> &args = m_parameter->GetParam("translation-details");
    if (args.size() == 1) {
      m_detailedTranslationReportingFilePath = args[0];
    } else {
      UserMessage::Add(string("the translation-details option requires exactly one filename argument"));
      return false;
    }
  }

  // word penalties
  for (size_t i = 0; i < m_parameter->GetParam("weight-w").size(); ++i) {
    float weightWordPenalty       = Scan<float>( m_parameter->GetParam("weight-w")[i] );
    m_wordPenaltyProducers.push_back(new WordPenaltyProducer(m_scoreIndexManager));
    m_allWeights.push_back(weightWordPenalty);
  }


  float weightUnknownWord				= (m_parameter->GetParam("weight-u").size() > 0) ? Scan<float>(m_parameter->GetParam("weight-u")[0]) : 1;
  m_unknownWordPenaltyProducer = new UnknownWordPenaltyProducer(m_scoreIndexManager);
  m_allWeights.push_back(weightUnknownWord);

  // reordering constraints
  m_maxDistortion = (m_parameter->GetParam("distortion-limit").size() > 0) ?
                    Scan<int>(m_parameter->GetParam("distortion-limit")[0])
                    : -1;
  SetBooleanParameter( &m_reorderingConstraint, "monotone-at-punctuation", false );

  // settings for pruning
  m_maxHypoStackSize = (m_parameter->GetParam("stack").size() > 0)
                       ? Scan<size_t>(m_parameter->GetParam("stack")[0]) : DEFAULT_MAX_HYPOSTACK_SIZE;
  m_minHypoStackDiversity = 0;
  if (m_parameter->GetParam("stack-diversity").size() > 0) {
    if (m_maxDistortion > 15) {
      UserMessage::Add("stack diversity > 0 is not allowed for distortion limits larger than 15");
      return false;
    }
    if (m_inputType == WordLatticeInput) {
      UserMessage::Add("stack diversity > 0 is not allowed for lattice input");
      return false;
    }
    m_minHypoStackDiversity = Scan<size_t>(m_parameter->GetParam("stack-diversity")[0]);
  }

  m_beamWidth = (m_parameter->GetParam("beam-threshold").size() > 0) ?
                TransformScore(Scan<float>(m_parameter->GetParam("beam-threshold")[0]))
                : TransformScore(DEFAULT_BEAM_WIDTH);
  m_earlyDiscardingThreshold = (m_parameter->GetParam("early-discarding-threshold").size() > 0) ?
                               TransformScore(Scan<float>(m_parameter->GetParam("early-discarding-threshold")[0]))
                               : TransformScore(DEFAULT_EARLY_DISCARDING_THRESHOLD);
  m_translationOptionThreshold = (m_parameter->GetParam("translation-option-threshold").size() > 0) ?
                                 TransformScore(Scan<float>(m_parameter->GetParam("translation-option-threshold")[0]))
                                 : TransformScore(DEFAULT_TRANSLATION_OPTION_THRESHOLD);

  m_maxNoTransOptPerCoverage = (m_parameter->GetParam("max-trans-opt-per-coverage").size() > 0)
                               ? Scan<size_t>(m_parameter->GetParam("max-trans-opt-per-coverage")[0]) : DEFAULT_MAX_TRANS_OPT_SIZE;

  m_maxNoPartTransOpt = (m_parameter->GetParam("max-partial-trans-opt").size() > 0)
                        ? Scan<size_t>(m_parameter->GetParam("max-partial-trans-opt")[0]) : DEFAULT_MAX_PART_TRANS_OPT_SIZE;

  m_maxPhraseLength = (m_parameter->GetParam("max-phrase-length").size() > 0)
                      ? Scan<size_t>(m_parameter->GetParam("max-phrase-length")[0]) : DEFAULT_MAX_PHRASE_LENGTH;

  m_cubePruningPopLimit = (m_parameter->GetParam("cube-pruning-pop-limit").size() > 0)
                          ? Scan<size_t>(m_parameter->GetParam("cube-pruning-pop-limit")[0]) : DEFAULT_CUBE_PRUNING_POP_LIMIT;

  m_cubePruningDiversity = (m_parameter->GetParam("cube-pruning-diversity").size() > 0)
                           ? Scan<size_t>(m_parameter->GetParam("cube-pruning-diversity")[0]) : DEFAULT_CUBE_PRUNING_DIVERSITY;

  SetBooleanParameter(&m_cubePruningLazyScoring, "cube-pruning-lazy-scoring", false);

  // unknown word processing
  SetBooleanParameter( &m_dropUnknown, "drop-unknown", false );

  SetBooleanParameter( &m_lmEnableOOVFeature, "lmodel-oov-feature", false);

  // minimum Bayes risk decoding
  SetBooleanParameter( &m_mbr, "minimum-bayes-risk", false );
  m_mbrSize = (m_parameter->GetParam("mbr-size").size() > 0) ?
              Scan<size_t>(m_parameter->GetParam("mbr-size")[0]) : 200;
  m_mbrScale = (m_parameter->GetParam("mbr-scale").size() > 0) ?
               Scan<float>(m_parameter->GetParam("mbr-scale")[0]) : 1.0f;

  //lattice mbr
  SetBooleanParameter( &m_useLatticeMBR, "lminimum-bayes-risk", false );
  if (m_useLatticeMBR && m_mbr) {
    cerr << "Errror: Cannot use both n-best mbr and lattice mbr together" << endl;
    exit(1);
  }

  if (m_useLatticeMBR) m_mbr = true;

  m_lmbrPruning = (m_parameter->GetParam("lmbr-pruning-factor").size() > 0) ?
                  Scan<size_t>(m_parameter->GetParam("lmbr-pruning-factor")[0]) : 30;
  m_lmbrThetas = Scan<float>(m_parameter->GetParam("lmbr-thetas"));
  SetBooleanParameter( &m_useLatticeHypSetForLatticeMBR, "lattice-hypo-set", false );
  m_lmbrPrecision = (m_parameter->GetParam("lmbr-p").size() > 0) ?
                    Scan<float>(m_parameter->GetParam("lmbr-p")[0]) : 0.8f;
  m_lmbrPRatio = (m_parameter->GetParam("lmbr-r").size() > 0) ?
                 Scan<float>(m_parameter->GetParam("lmbr-r")[0]) : 0.6f;
  m_lmbrMapWeight = (m_parameter->GetParam("lmbr-map-weight").size() >0) ?
                    Scan<float>(m_parameter->GetParam("lmbr-map-weight")[0]) : 0.0f;

  //consensus decoding
  SetBooleanParameter( &m_useConsensusDecoding, "consensus-decoding", false );
  if (m_useConsensusDecoding && m_mbr) {
    cerr<< "Error: Cannot use consensus decoding together with mbr" << endl;
    exit(1);
  }
  if (m_useConsensusDecoding) m_mbr=true;


  m_timeout_threshold = (m_parameter->GetParam("time-out").size() > 0) ?
                        Scan<size_t>(m_parameter->GetParam("time-out")[0]) : -1;
  m_timeout = (GetTimeoutThreshold() == (size_t)-1) ? false : true;


  m_lmcache_cleanup_threshold = (m_parameter->GetParam("clean-lm-cache").size() > 0) ?
                                Scan<size_t>(m_parameter->GetParam("clean-lm-cache")[0]) : 1;

  m_threadCount = 1;
  const std::vector<std::string> &threadInfo = m_parameter->GetParam("threads");
  if (!threadInfo.empty()) {
    if (threadInfo[0] == "all") {
#ifdef WITH_THREADS
      m_threadCount = boost::thread::hardware_concurrency();
      if (!m_threadCount) {
        UserMessage::Add("-threads all specified but Boost doesn't know how many cores there are");
        return false;
      }
#else
      UserMessage::Add("-threads all specified but moses not built with thread support");
      return false;
#endif
    } else {
      m_threadCount = Scan<int>(threadInfo[0]);
      if (m_threadCount < 1) {
        UserMessage::Add("Specify at least one thread.");
        return false;
      }
#ifndef WITH_THREADS
      if (m_threadCount > 1) {
        UserMessage::Add(std::string("Error: Thread count of ") + threadInfo[0] + " but moses not built with thread support");
        return false;
      }
#endif
    }
  }

  // Read in constraint decoding file, if provided
  if(m_parameter->GetParam("constraint").size()) {
    if (m_parameter->GetParam("search-algorithm").size() > 0
        && Scan<size_t>(m_parameter->GetParam("search-algorithm")[0]) != 0) {
      cerr << "Can use -constraint only with stack-based search (-search-algorithm 0)" << endl;
      exit(1);
    }
    m_constraintFileName = m_parameter->GetParam("constraint")[0];

    InputFileStream constraintFile(m_constraintFileName);

    std::string line;

    long sentenceID = -1;
    while (getline(constraintFile, line)) {
      vector<string> vecStr = Tokenize(line, "\t");

      if (vecStr.size() == 1) {
        sentenceID++;
        Phrase phrase(Output, 0);
        phrase.CreateFromString(GetOutputFactorOrder(), vecStr[0], GetFactorDelimiter());
        m_constraints.insert(make_pair(sentenceID,phrase));
      } else if (vecStr.size() == 2) {
        sentenceID = Scan<long>(vecStr[0]);
        Phrase phrase(Output, 0);
        phrase.CreateFromString(GetOutputFactorOrder(), vecStr[1], GetFactorDelimiter());
        m_constraints.insert(make_pair(sentenceID,phrase));
      } else {
        assert(false);
      }
    }
  }

  // use of xml in input
  if (m_parameter->GetParam("xml-input").size() == 0) m_xmlInputType = XmlPassThrough;
  else if (m_parameter->GetParam("xml-input")[0]=="exclusive") m_xmlInputType = XmlExclusive;
  else if (m_parameter->GetParam("xml-input")[0]=="inclusive") m_xmlInputType = XmlInclusive;
  else if (m_parameter->GetParam("xml-input")[0]=="ignore") m_xmlInputType = XmlIgnore;
  else if (m_parameter->GetParam("xml-input")[0]=="pass-through") m_xmlInputType = XmlPassThrough;
  else {
    UserMessage::Add("invalid xml-input value, must be pass-through, exclusive, inclusive, or ignore");
    return false;
  }

#ifdef HAVE_SYNLM
	if (m_parameter->GetParam("slmodel-file").size() > 0) {
	  if (!LoadSyntacticLanguageModel()) return false;
	}
#endif
	
  if (!LoadLexicalReorderingModel()) return false;
  if (!LoadLanguageModels()) return false;
  if (!LoadGenerationTables()) return false;
  if (!LoadPhraseTables()) return false;
  if (!LoadGlobalLexicalModel()) return false;
  if (!LoadDecodeGraphs()) return false;


  //configure the translation systems with these tables
  vector<string> tsConfig = m_parameter->GetParam("translation-systems");
  if (!tsConfig.size()) {
    //use all models in default system.
    tsConfig.push_back(TranslationSystem::DEFAULT + " R * D * L * G *");
  }

  if (m_wordPenaltyProducers.size() != tsConfig.size()) {
    UserMessage::Add(string("Mismatch between number of word penalties and number of translation systems"));
    return false;
  }

  if (m_searchAlgorithm == ChartDecoding) {
    //insert some null distortion score producers
    m_distortionScoreProducers.assign(tsConfig.size(), NULL);
  } else {
    if (m_distortionScoreProducers.size() != tsConfig.size()) {
      UserMessage::Add(string("Mismatch between number of distortion scores and number of translation systems. Or [search-algorithm] has been set to a phrase-based algorithm when it should be chart decoding"));
      return false;
    }
  }

  for (size_t i = 0; i < tsConfig.size(); ++i) {
    vector<string> config = Tokenize(tsConfig[i]);
    if (config.size() % 2 != 1) {
      UserMessage::Add(string("Incorrect number of fields in Translation System config. Should be an odd number"));
    }
    m_translationSystems.insert(pair<string, TranslationSystem>(config[0],
                                TranslationSystem(config[0],m_wordPenaltyProducers[i],m_unknownWordPenaltyProducer,m_distortionScoreProducers[i])));
    for (size_t j = 1; j < config.size(); j += 2) {
      const string& id = config[j];
      const string& tables = config[j+1];
      set<size_t> tableIds;
      if (tables != "*") {
        //selected tables
        vector<string> tableIdStrings = Tokenize(tables,",");
        vector<size_t> tableIdList;
        Scan<size_t>(tableIdList, tableIdStrings);
        copy(tableIdList.begin(), tableIdList.end(), inserter(tableIds,tableIds.end()));
      }
      if (id == "D") {
        for (size_t k = 0; k < m_decodeGraphs.size(); ++k) {
          if (!tableIds.size() || tableIds.find(k) != tableIds.end()) {
            VERBOSE(2,"Adding decoder graph " << k << " to translation system " << config[0] << endl);
            m_translationSystems.find(config[0])->second.AddDecodeGraph(m_decodeGraphs[k],m_decodeGraphBackoff[k]);
          }
        }
      } else if (id == "R") {
        for (size_t k = 0; k < m_reorderModels.size(); ++k) {
          if (!tableIds.size() || tableIds.find(k) != tableIds.end()) {
            m_translationSystems.find(config[0])->second.AddReorderModel(m_reorderModels[k]);
            VERBOSE(2,"Adding reorder table " << k << " to translation system " << config[0] << endl);
          }
        }
      } else if (id == "G") {
        for (size_t k = 0; k < m_globalLexicalModels.size(); ++k) {
          if (!tableIds.size() || tableIds.find(k) != tableIds.end()) {
            m_translationSystems.find(config[0])->second.AddGlobalLexicalModel(m_globalLexicalModels[k]);
            VERBOSE(2,"Adding global lexical model " << k << " to translation system " << config[0] << endl);
          }
        }
      } else if (id == "L") {
        size_t lmid = 0;
        for (LMList::const_iterator k = m_languageModel.begin(); k != m_languageModel.end(); ++k, ++lmid) {
          if (!tableIds.size() || tableIds.find(lmid) != tableIds.end()) {
            m_translationSystems.find(config[0])->second.AddLanguageModel(*k);
            VERBOSE(2,"Adding language model " << lmid << " to translation system " << config[0] << endl);
          }
        }
      } else {
        UserMessage::Add(string("Incorrect translation system identifier: ") + id);
        return false;
      }
    }
    //Instigate dictionary loading
    m_translationSystems.find(config[0])->second.ConfigDictionaries();



    //Add any other features here.
#ifdef HAVE_SYNLM
    if (m_syntacticLanguageModel != NULL) {
      m_translationSystems.find(config[0])->second.AddFeatureFunction(m_syntacticLanguageModel);
    }
#endif
  }


  m_scoreIndexManager.InitFeatureNames();

  return true;
}

void StaticData::SetBooleanParameter( bool *parameter, string parameterName, bool defaultValue )
{
  // default value if nothing is specified
  *parameter = defaultValue;
  if (! m_parameter->isParamSpecified( parameterName ) ) {
    return;
  }

  // if parameter is just specified as, e.g. "-parameter" set it true
  if (m_parameter->GetParam( parameterName ).size() == 0) {
    *parameter = true;
  }

  // if paramter is specified "-parameter true" or "-parameter false"
  else if (m_parameter->GetParam( parameterName ).size() == 1) {
    *parameter = Scan<bool>( m_parameter->GetParam( parameterName )[0]);
  }
}

StaticData::~StaticData()
{
  RemoveAllInColl(m_phraseDictionary);
  RemoveAllInColl(m_generationDictionary);
  RemoveAllInColl(m_reorderModels);
  RemoveAllInColl(m_globalLexicalModels);
	
#ifdef HAVE_SYNLM
	delete m_syntacticLanguageModel;
#endif


  RemoveAllInColl(m_decodeGraphs);
  RemoveAllInColl(m_wordPenaltyProducers);
  RemoveAllInColl(m_distortionScoreProducers);
  m_languageModel.CleanUp();

  // delete trans opt
  ClearTransOptionCache();

  // small score producers
  delete m_unknownWordPenaltyProducer;

  // MJD: Was commented out, uncommented it, but do not remember why.
  // Probably I had a good reason. TODO: Check reason :)
 delete m_parameter;

  // memory pools
  Phrase::FinalizeMemPool();

}

#ifdef HAVE_SYNLM
  bool StaticData::LoadSyntacticLanguageModel() {
    cerr << "Loading syntactic language models..." << std::endl;
    
    const vector<float> weights = Scan<float>(m_parameter->GetParam("weight-slm"));
    const vector<string> files = m_parameter->GetParam("slmodel-file");
    
    const FactorType factorType = (m_parameter->GetParam("slmodel-factor").size() > 0) ?
      TransformScore(Scan<int>(m_parameter->GetParam("slmodel-factor")[0]))
      : 0;

    const size_t beamWidth = (m_parameter->GetParam("slmodel-beam").size() > 0) ?
      TransformScore(Scan<int>(m_parameter->GetParam("slmodel-beam")[0]))
      : 500;

    if (files.size() < 1) {
      cerr << "No syntactic language model files specified!" << std::endl;
      return false;
    }

    // check if feature is used
    if (weights.size() >= 1) {

      //cout.setf(ios::scientific,ios::floatfield);
      //cerr.setf(ios::scientific,ios::floatfield);
      
      // create the feature
      m_syntacticLanguageModel = new SyntacticLanguageModel(files,weights,factorType,beamWidth); 

      /* 
      /////////////////////////////////////////
      // BEGIN LANE's UNSTABLE EXPERIMENT :)
      //

      double ppl = m_syntacticLanguageModel->perplexity();
      cerr << "Probability is " << ppl << endl;


      //
      // END LANE's UNSTABLE EXPERIMENT
      /////////////////////////////////////////
      */


      if (m_syntacticLanguageModel==NULL) {
	return false;
      }

    }
    
    return true;

  }
#endif

bool StaticData::LoadLexicalReorderingModel()
{
  VERBOSE(1, "Loading lexical distortion models...");
  const vector<string> fileStr    = m_parameter->GetParam("distortion-file");
  bool hasWeightlr = (m_parameter->GetParam("weight-lr").size() != 0);
  vector<string> weightsStr;
  if (hasWeightlr) {
    weightsStr = m_parameter->GetParam("weight-lr");
  } else {
    weightsStr = m_parameter->GetParam("weight-d");
  }

  std::vector<float>   weights;
  size_t w = 1; //cur weight
  if (hasWeightlr) {
    w = 0; // if reading from weight-lr, don't have to count first as distortion penalty
  }
  size_t f = 0; //cur file
  //get weights values
  VERBOSE(1, "have " << fileStr.size() << " models" << std::endl);
  for(size_t j = 0; j < weightsStr.size(); ++j) {
    weights.push_back(Scan<float>(weightsStr[j]));
  }
  //load all models
  for(size_t i = 0; i < fileStr.size(); ++i) {
    vector<string> spec = Tokenize<string>(fileStr[f], " ");
    ++f; //mark file as consumed
    if(spec.size() != 4) {
      UserMessage::Add("Invalid Lexical Reordering Model Specification: " + fileStr[f]);
      return false;
    }

    // spec[0] = factor map
    // spec[1] = name
    // spec[2] = num weights
    // spec[3] = fileName

    // decode factor map

    vector<FactorType> input, output;
    vector<string> inputfactors = Tokenize(spec[0],"-");
    if(inputfactors.size() == 2) {
      input  = Tokenize<FactorType>(inputfactors[0],",");
      output = Tokenize<FactorType>(inputfactors[1],",");
    } else if(inputfactors.size() == 1) {
      //if there is only one side assume it is on e side... why?
      output = Tokenize<FactorType>(inputfactors[0],",");
    } else {
      //format error
      return false;
    }

    string modelType = spec[1];

    // decode num weights and fetch weights from array
    std::vector<float> mweights;
    size_t numWeights = atoi(spec[2].c_str());
    for(size_t k = 0; k < numWeights; ++k, ++w) {
      if(w >= weights.size()) {
        UserMessage::Add("Lexicalized distortion model: Not enough weights, add to [weight-d]");
        return false;
      } else
        mweights.push_back(weights[w]);
    }

    string filePath = spec[3];

    m_reorderModels.push_back(new LexicalReordering(input, output, modelType, filePath, mweights));
  }
  return true;
}

bool StaticData::LoadGlobalLexicalModel()
{
  const vector<float> &weight = Scan<float>(m_parameter->GetParam("weight-lex"));
  const vector<string> &file = m_parameter->GetParam("global-lexical-file");

  if (weight.size() != file.size()) {
    std::cerr << "number of weights and models for the global lexical model does not match ("
              << weight.size() << " != " << file.size() << ")" << std::endl;
    return false;
  }

  for (size_t i = 0; i < weight.size(); i++ ) {
    vector<string> spec = Tokenize<string>(file[i], " ");
    if ( spec.size() != 2 ) {
      std::cerr << "wrong global lexical model specification: " << file[i] << endl;
      return false;
    }
    vector< string > factors = Tokenize(spec[0],"-");
    if ( factors.size() != 2 ) {
      std::cerr << "wrong factor definition for global lexical model: " << spec[0] << endl;
      return false;
    }
    vector<FactorType> inputFactors = Tokenize<FactorType>(factors[0],",");
    vector<FactorType> outputFactors = Tokenize<FactorType>(factors[1],",");
    m_globalLexicalModels.push_back( new GlobalLexicalModel( spec[1], weight[i], inputFactors, outputFactors ) );
  }
  return true;
}

bool StaticData::LoadLanguageModels()
{
  if (m_parameter->GetParam("lmodel-file").size() > 0) {
    // weights
    vector<float> weightAll = Scan<float>(m_parameter->GetParam("weight-l"));

    for (size_t i = 0 ; i < weightAll.size() ; i++) {
      m_allWeights.push_back(weightAll[i]);
    }

    // dictionary upper-bounds fo all IRST LMs
    vector<int> LMdub = Scan<int>(m_parameter->GetParam("lmodel-dub"));
    if (m_parameter->GetParam("lmodel-dub").size() == 0) {
      for(size_t i=0; i<m_parameter->GetParam("lmodel-file").size(); i++)
        LMdub.push_back(0);
    }

    // initialize n-gram order for each factor. populated only by factored lm
    const vector<string> &lmVector = m_parameter->GetParam("lmodel-file");
    //prevent language models from being loaded twice
    map<string,LanguageModel*> languageModelsLoaded;

    for(size_t i=0; i<lmVector.size(); i++) {
      LanguageModel* lm = NULL;
      if (languageModelsLoaded.find(lmVector[i]) != languageModelsLoaded.end()) {
        lm = new LanguageModel(m_scoreIndexManager, languageModelsLoaded[lmVector[i]]);
      } else {
        vector<string>	token		= Tokenize(lmVector[i]);
        if (token.size() != 4 && token.size() != 5 ) {
          UserMessage::Add("Expected format 'LM-TYPE FACTOR-TYPE NGRAM-ORDER filePath [mapFilePath (only for IRSTLM)]'");
          return false;
        }
        // type = implementation, SRI, IRST etc
        LMImplementation lmImplementation = static_cast<LMImplementation>(Scan<int>(token[0]));

        // factorType = 0 = Surface, 1 = POS, 2 = Stem, 3 = Morphology, etc
        vector<FactorType> 	factorTypes		= Tokenize<FactorType>(token[1], ",");

        // nGramOrder = 2 = bigram, 3 = trigram, etc
        size_t nGramOrder = Scan<int>(token[2]);

        string &languageModelFile = token[3];
        if (token.size() == 5) {
          if (lmImplementation==IRST)
            languageModelFile += " " + token[4];
          else {
            UserMessage::Add("Expected format 'LM-TYPE FACTOR-TYPE NGRAM-ORDER filePath [mapFilePath (only for IRSTLM)]'");
            return false;
          }
        }
        IFVERBOSE(1)
        PrintUserTime(string("Start loading LanguageModel ") + languageModelFile);

        lm = LanguageModelFactory::CreateLanguageModel(
               lmImplementation
               , factorTypes
               , nGramOrder
               , languageModelFile
               , m_scoreIndexManager
               , LMdub[i]);
        if (lm == NULL) {
          UserMessage::Add("no LM created. We probably don't have it compiled");
          return false;
        }
        languageModelsLoaded[lmVector[i]] = lm;
      }

      m_languageModel.Add(lm);
    }
  }
  // flag indicating that language models were loaded,
  // since phrase table loading requires their presence
  m_fLMsLoaded = true;
  IFVERBOSE(1)
  PrintUserTime("Finished loading LanguageModels");
  return true;
}

bool StaticData::LoadGenerationTables()
{
  if (m_parameter->GetParam("generation-file").size() > 0) {
    const vector<string> &generationVector = m_parameter->GetParam("generation-file");
    const vector<float> &weight = Scan<float>(m_parameter->GetParam("weight-generation"));

    IFVERBOSE(1) {
      TRACE_ERR( "weight-generation: ");
      for (size_t i = 0 ; i < weight.size() ; i++) {
        TRACE_ERR( weight[i] << "\t");
      }
      TRACE_ERR(endl);
    }
    size_t currWeightNum = 0;

    for(size_t currDict = 0 ; currDict < generationVector.size(); currDict++) {
      vector<string>			token		= Tokenize(generationVector[currDict]);
      vector<FactorType> 	input		= Tokenize<FactorType>(token[0], ",")
                                    ,output	= Tokenize<FactorType>(token[1], ",");
      m_maxFactorIdx[1] = CalcMax(m_maxFactorIdx[1], input, output);
      string							filePath;
      size_t							numFeatures;

      numFeatures = Scan<size_t>(token[2]);
      filePath = token[3];

      if (!FileExists(filePath) && FileExists(filePath + ".gz")) {
        filePath += ".gz";
      }

      VERBOSE(1, filePath << endl);

      m_generationDictionary.push_back(new GenerationDictionary(numFeatures, m_scoreIndexManager, input,output));
      assert(m_generationDictionary.back() && "could not create GenerationDictionary");
      if (!m_generationDictionary.back()->Load(filePath, Output)) {
        delete m_generationDictionary.back();
        return false;
      }
      for(size_t i = 0; i < numFeatures; i++) {
        assert(currWeightNum < weight.size());
        m_allWeights.push_back(weight[currWeightNum++]);
      }
    }
    if (currWeightNum != weight.size()) {
      TRACE_ERR( "  [WARNING] config file has " << weight.size() << " generation weights listed, but the configuration for generation files indicates there should be " << currWeightNum << "!\n");
    }
  }

  return true;
}

/* Doesn't load phrase tables any more. Just creates the features. */
bool StaticData::LoadPhraseTables()
{
  VERBOSE(2,"Creating phrase table features" << endl);

  // language models must be loaded prior to loading phrase tables
  assert(m_fLMsLoaded);
  // load phrase translation tables
  if (m_parameter->GetParam("ttable-file").size() > 0) {
    // weights
    vector<float> weightAll									= Scan<float>(m_parameter->GetParam("weight-t"));

    const vector<string> &translationVector = m_parameter->GetParam("ttable-file");
    vector<size_t>	maxTargetPhrase					= Scan<size_t>(m_parameter->GetParam("ttable-limit"));

    if(maxTargetPhrase.size() == 1 && translationVector.size() > 1) {
      VERBOSE(1, "Using uniform ttable-limit of " << maxTargetPhrase[0] << " for all translation tables." << endl);
      for(size_t i = 1; i < translationVector.size(); i++)
        maxTargetPhrase.push_back(maxTargetPhrase[0]);
    } else if(maxTargetPhrase.size() != 1 && maxTargetPhrase.size() < translationVector.size()) {
      stringstream strme;
      strme << "You specified " << translationVector.size() << " translation tables, but only " << maxTargetPhrase.size() << " ttable-limits.";
      UserMessage::Add(strme.str());
      return false;
    }

    size_t index = 0;
    size_t weightAllOffset = 0;
    bool oldFileFormat = false;
    for(size_t currDict = 0 ; currDict < translationVector.size(); currDict++) {
      vector<string>                  token           = Tokenize(translationVector[currDict]);

      if(currDict == 0 && token.size() == 4) {
        VERBOSE(1, "Warning: Phrase table specification in old 4-field format. Assuming binary phrase tables (type 1)!" << endl);
        oldFileFormat = true;
      }

      if((!oldFileFormat && token.size() < 5) || (oldFileFormat && token.size() != 4)) {
        UserMessage::Add("invalid phrase table specification");
        return false;
      }

      PhraseTableImplementation implementation = (PhraseTableImplementation) Scan<int>(token[0]);
      if(oldFileFormat) {
        token.push_back(token[3]);
        token[3] = token[2];
        token[2] = token[1];
        token[1] = token[0];
        token[0] = "1";
        implementation = Binary;
      } else
        implementation = (PhraseTableImplementation) Scan<int>(token[0]);

      assert(token.size() >= 5);
      //characteristics of the phrase table

      vector<FactorType>  input		= Tokenize<FactorType>(token[1], ",")
                                    ,output = Tokenize<FactorType>(token[2], ",");
      m_maxFactorIdx[0] = CalcMax(m_maxFactorIdx[0], input);
      m_maxFactorIdx[1] = CalcMax(m_maxFactorIdx[1], output);
      m_maxNumFactors = std::max(m_maxFactorIdx[0], m_maxFactorIdx[1]) + 1;
      size_t numScoreComponent = Scan<size_t>(token[3]);
      string filePath= token[4];

      assert(weightAll.size() >= weightAllOffset + numScoreComponent);

      // weights for this phrase dictionary
      // first InputScores (if any), then translation scores
      vector<float> weight;

      if(currDict==0 && (m_inputType == ConfusionNetworkInput || m_inputType == WordLatticeInput)) {
        // TODO. find what the assumptions made by confusion network about phrase table output which makes
        // it only work with binrary file. This is a hack

        m_numInputScores=m_parameter->GetParam("weight-i").size();
        for(unsigned k=0; k<m_numInputScores; ++k)
          weight.push_back(Scan<float>(m_parameter->GetParam("weight-i")[k]));

        if(m_parameter->GetParam("link-param-count").size())
          m_numLinkParams = Scan<size_t>(m_parameter->GetParam("link-param-count")[0]);

        //print some info about this interaction:
        if (m_numLinkParams == m_numInputScores) {
          VERBOSE(1,"specified equal numbers of link parameters and insertion weights, not using non-epsilon 'real' word link count.\n");
        } else if ((m_numLinkParams + 1) == m_numInputScores) {
          VERBOSE(1,"WARN: "<< m_numInputScores << " insertion weights found and only "<< m_numLinkParams << " link parameters specified, applying non-epsilon 'real' word link count for last feature weight.\n");
        } else {
          stringstream strme;
          strme << "You specified " << m_numInputScores
                << " input weights (weight-i), but you specified " << m_numLinkParams << " link parameters (link-param-count)!";
          UserMessage::Add(strme.str());
          return false;
        }

      }
      if (!m_inputType) {
        m_numInputScores=0;
      }
      //this number changes depending on what phrase table we're talking about: only 0 has the weights on it
      size_t tableInputScores = (currDict == 0 ? m_numInputScores : 0);

      for (size_t currScore = 0 ; currScore < numScoreComponent; currScore++)
        weight.push_back(weightAll[weightAllOffset + currScore]);


      if(weight.size() - tableInputScores != numScoreComponent) {
        stringstream strme;
        strme << "Your phrase table has " << numScoreComponent
              << " scores, but you specified " << (weight.size() - tableInputScores) << " weights!";
        UserMessage::Add(strme.str());
        return false;
      }

      weightAllOffset += numScoreComponent;
      numScoreComponent += tableInputScores;

      string targetPath, alignmentsFile;
      if (implementation == SuffixArray) {
        targetPath		= token[5];
        alignmentsFile= token[6];
      }

      assert(numScoreComponent==weight.size());

      std::copy(weight.begin(),weight.end(),std::back_inserter(m_allWeights));

      //This is needed for regression testing, but the phrase table
      //might not really be loading here
      IFVERBOSE(1)
      PrintUserTime(string("Start loading PhraseTable ") + filePath);
      VERBOSE(1,"filePath: " << filePath <<endl);

      PhraseDictionaryFeature* pdf = new PhraseDictionaryFeature(
        implementation
        , numScoreComponent
        , (currDict==0 ? m_numInputScores : 0)
        , input
        , output
        , filePath
        , weight
        , maxTargetPhrase[index]
        , targetPath, alignmentsFile);

      m_phraseDictionary.push_back(pdf);





      index++;
    }
  }

  IFVERBOSE(1)
  PrintUserTime("Finished loading phrase tables");
  return true;
}

void StaticData::LoadNonTerminals()
{
  string defaultNonTerminals;

  if (m_parameter->GetParam("non-terminals").size() == 0) {
    defaultNonTerminals = "X";
  } else {
    vector<std::string> tokens = Tokenize(m_parameter->GetParam("non-terminals")[0]);
    defaultNonTerminals = tokens[0];
  }

  FactorCollection &factorCollection = FactorCollection::Instance();

  m_inputDefaultNonTerminal.SetIsNonTerminal(true);
  const Factor *sourceFactor = factorCollection.AddFactor(Input, 0, defaultNonTerminals);
  m_inputDefaultNonTerminal.SetFactor(0, sourceFactor);

  m_outputDefaultNonTerminal.SetIsNonTerminal(true);
  const Factor *targetFactor = factorCollection.AddFactor(Output, 0, defaultNonTerminals);
  m_outputDefaultNonTerminal.SetFactor(0, targetFactor);

  // for unknwon words
  if (m_parameter->GetParam("unknown-lhs").size() == 0) {
    UnknownLHSEntry entry(defaultNonTerminals, 0.0f);
    m_unknownLHS.push_back(entry);
  } else {
    const string &filePath = m_parameter->GetParam("unknown-lhs")[0];

    InputFileStream inStream(filePath);
    string line;
    while(getline(inStream, line)) {
      vector<string> tokens = Tokenize(line);
      assert(tokens.size() == 2);
      UnknownLHSEntry entry(tokens[0], Scan<float>(tokens[1]));
      m_unknownLHS.push_back(entry);
    }

  }

}

void StaticData::LoadChartDecodingParameters()
{
  LoadNonTerminals();

  // source label overlap
  if (m_parameter->GetParam("source-label-overlap").size() > 0) {
    m_sourceLabelOverlap = (SourceLabelOverlap) Scan<int>(m_parameter->GetParam("source-label-overlap")[0]);
  } else {
    m_sourceLabelOverlap = SourceLabelOverlapAdd;
  }

  m_ruleLimit = (m_parameter->GetParam("rule-limit").size() > 0)
                ? Scan<size_t>(m_parameter->GetParam("rule-limit")[0]) : DEFAULT_MAX_TRANS_OPT_SIZE;
}

void StaticData::LoadPhraseBasedParameters()
{
  const vector<string> distortionWeights = m_parameter->GetParam("weight-d");
  size_t distortionWeightCount = distortionWeights.size();
  //if there's a lex-reordering model, and no separate weight set, then
  //take just one of these weights for linear distortion
  if (!m_parameter->GetParam("weight-lr").size() && m_parameter->GetParam("distortion-file").size()) {
    distortionWeightCount = 1;
  }
  for (size_t i = 0; i < distortionWeightCount; ++i) {
    float weightDistortion = Scan<float>(distortionWeights[i]);
    m_distortionScoreProducers.push_back(new DistortionScoreProducer(m_scoreIndexManager));
    m_allWeights.push_back(weightDistortion);
  }
}

bool StaticData::LoadDecodeGraphs()
{
  const vector<string> &mappingVector = m_parameter->GetParam("mapping");
  const vector<size_t> &maxChartSpans = Scan<size_t>(m_parameter->GetParam("max-chart-span"));

  DecodeStep *prev = 0;
  size_t prevDecodeGraphInd = 0;
  for(size_t i=0; i<mappingVector.size(); i++) {
    vector<string>	token		= Tokenize(mappingVector[i]);
    size_t decodeGraphInd;
    DecodeType decodeType;
    size_t index;
    if (token.size() == 2) {
      decodeGraphInd = 0;
      decodeType = token[0] == "T" ? Translate : Generate;
      index = Scan<size_t>(token[1]);
    } else if (token.size() == 3) {
      // For specifying multiple translation model
      decodeGraphInd = Scan<size_t>(token[0]);
      //the vectorList index can only increment by one
      assert(decodeGraphInd == prevDecodeGraphInd || decodeGraphInd == prevDecodeGraphInd + 1);
      if (decodeGraphInd > prevDecodeGraphInd) {
        prev = NULL;
      }
      decodeType = token[1] == "T" ? Translate : Generate;
      index = Scan<size_t>(token[2]);
    } else {
      UserMessage::Add("Malformed mapping!");
      assert(false);
    }

    DecodeStep* decodeStep = NULL;
    switch (decodeType) {
    case Translate:
      if(index>=m_phraseDictionary.size()) {
        stringstream strme;
        strme << "No phrase dictionary with index "
              << index << " available!";
        UserMessage::Add(strme.str());
        assert(false);
      }
      decodeStep = new DecodeStepTranslation(m_phraseDictionary[index], prev);
      break;
    case Generate:
      if(index>=m_generationDictionary.size()) {
        stringstream strme;
        strme << "No generation dictionary with index "
              << index << " available!";
        UserMessage::Add(strme.str());
        assert(false);
      }
      decodeStep = new DecodeStepGeneration(m_generationDictionary[index], prev);
      break;
    case InsertNullFertilityWord:
      assert(!"Please implement NullFertilityInsertion.");
      break;
    }

    assert(decodeStep);
    if (m_decodeGraphs.size() < decodeGraphInd + 1) {
      DecodeGraph *decodeGraph;
      if (m_searchAlgorithm == ChartDecoding) {
        size_t maxChartSpan = (decodeGraphInd < maxChartSpans.size()) ? maxChartSpans[decodeGraphInd] : DEFAULT_MAX_CHART_SPAN;
        decodeGraph = new DecodeGraph(m_decodeGraphs.size(), maxChartSpan);
      } else {
        decodeGraph = new DecodeGraph(m_decodeGraphs.size());
      }

      m_decodeGraphs.push_back(decodeGraph); // TODO max chart span
    }

    m_decodeGraphs[decodeGraphInd]->Add(decodeStep);
    prev = decodeStep;
    prevDecodeGraphInd = decodeGraphInd;
  }

  // set maximum n-gram size for backoff approach to decoding paths
  // default is always use subsequent paths (value = 0)
  for(size_t i=0; i<m_decodeGraphs.size(); i++) {
    m_decodeGraphBackoff.push_back( 0 );
  }
  // if specified, record maxmimum unseen n-gram size
  const vector<string> &backoffVector = m_parameter->GetParam("decoding-graph-backoff");
  for(size_t i=0; i<m_decodeGraphs.size() && i<backoffVector.size(); i++) {
    m_decodeGraphBackoff[i] = Scan<size_t>(backoffVector[i]);
  }

  return true;
}


void StaticData::SetWeightsForScoreProducer(const ScoreProducer* sp, const std::vector<float>& weights)
{
  const size_t id = sp->GetScoreBookkeepingID();
  const size_t begin = m_scoreIndexManager.GetBeginIndex(id);
  const size_t end = m_scoreIndexManager.GetEndIndex(id);
  assert(end - begin == weights.size());
  if (m_allWeights.size() < end)
    m_allWeights.resize(end);
  std::vector<float>::const_iterator weightIter = weights.begin();
  for (size_t i = begin; i < end; i++)
    m_allWeights[i] = *weightIter++;
}

const TranslationOptionList* StaticData::FindTransOptListInCache(const DecodeGraph &decodeGraph, const Phrase &sourcePhrase) const
{
  std::pair<size_t, Phrase> key(decodeGraph.GetPosition(), sourcePhrase);
#ifdef WITH_THREADS
  boost::mutex::scoped_lock lock(m_transOptCacheMutex);
#endif
  std::map<std::pair<size_t, Phrase>, std::pair<TranslationOptionList*,clock_t> >::iterator iter
  = m_transOptCache.find(key);
  if (iter == m_transOptCache.end())
    return NULL;
  iter->second.second = clock(); // update last used time
  return iter->second.first;
}

void StaticData::ReduceTransOptCache() const
{
  if (m_transOptCache.size() <= m_transOptCacheMaxSize) return; // not full
  clock_t t = clock();

  // find cutoff for last used time
  priority_queue< clock_t > lastUsedTimes;
  std::map<std::pair<size_t, Phrase>, std::pair<TranslationOptionList*,clock_t> >::iterator iter;
  iter = m_transOptCache.begin();
  while( iter != m_transOptCache.end() ) {
    lastUsedTimes.push( iter->second.second );
    iter++;
  }
  for( size_t i=0; i < lastUsedTimes.size()-m_transOptCacheMaxSize/2; i++ )
    lastUsedTimes.pop();
  clock_t cutoffLastUsedTime = lastUsedTimes.top();

  // remove all old entries
  iter = m_transOptCache.begin();
  while( iter != m_transOptCache.end() ) {
    if (iter->second.second < cutoffLastUsedTime) {
      std::map<std::pair<size_t, Phrase>, std::pair<TranslationOptionList*,clock_t> >::iterator iterRemove = iter++;
      delete iterRemove->second.first;
      m_transOptCache.erase(iterRemove);
    } else iter++;
  }
  VERBOSE(2,"Reduced persistent translation option cache in " << ((clock()-t)/(float)CLOCKS_PER_SEC) << " seconds." << std::endl);
}

void StaticData::AddTransOptListToCache(const DecodeGraph &decodeGraph, const Phrase &sourcePhrase, const TranslationOptionList &transOptList) const
{
  if (m_transOptCacheMaxSize == 0) return;
  std::pair<size_t, Phrase> key(decodeGraph.GetPosition(), sourcePhrase);
  TranslationOptionList* storedTransOptList = new TranslationOptionList(transOptList);
#ifdef WITH_THREADS
  boost::mutex::scoped_lock lock(m_transOptCacheMutex);
#endif
  m_transOptCache[key] = make_pair( storedTransOptList, clock() );
  ReduceTransOptCache();
}
void StaticData::ClearTransOptionCache() const {
  map<std::pair<size_t, Phrase>, std::pair< TranslationOptionList*, clock_t > >::iterator iterCache;
  for (iterCache = m_transOptCache.begin() ; iterCache != m_transOptCache.end() ; ++iterCache) {
    TranslationOptionList *transOptList = iterCache->second.first;
    delete transOptList;
  }
}

// MJD: This is where the magic happens!
const WeightInfos StaticData::SetTranslationSystemWeights(const TranslationSystem& system, const WeightInfos& newWeights)
{
  WeightInfos oldParams;
  for(WeightInfos::const_iterator it = newWeights.begin(); it != newWeights.end(); it++) {
    WeightInfo oldValue = SetTranslationSystemWeight(system, *it);
    oldParams.push_back(oldValue);
  }
  return oldParams;
}	

// MJD: some more magic!
WeightInfo StaticData::SetTranslationSystemWeight(const TranslationSystem& system, WeightInfo wi)
{
  WeightInfo oldWeight = wi;
  const unsigned int verbose = 2;
  VERBOSE(verbose, "Setting weight " << wi.GetDescription() << " for TranslationSystem " << system.GetId() << " to " << wi.value << std::endl);
      
  std::vector<const ScoreProducer*> sps;
  std::vector<const StatefulFeatureFunction*> sff = system.GetStatefulFeatureFunctions();
  for(size_t i = 0; i < sff.size(); i++) {
    if(sff[i]->GetScoreProducerWeightShortName() == wi.name)
      sps.push_back((ScoreProducer*) sff[i]);
  }
  std::vector<const StatelessFeatureFunction*> slf = system.GetStatelessFeatureFunctions();
  for(size_t i = 0; i < slf.size(); i++) {
    if(slf[i]->GetScoreProducerWeightShortName() == wi.name)
      sps.push_back((ScoreProducer*) slf[i]);
  }
  const vector<PhraseDictionaryFeature*>& pds = system.GetPhraseDictionaries();
  for(size_t i = 0; i < pds.size(); i++) {
    if(pds[i]->GetScoreProducerWeightShortName() == wi.name)
      sps.push_back((ScoreProducer*) pds[i]);
  }
  
  const vector<GenerationDictionary*>& gds = system.GetGenerationDictionaries();
  for(size_t i = 0; i < gds.size(); i++) {
    // MJD: Why suddenly a parameter in the new trunk version?
    if(gds[i]->GetScoreProducerWeightShortName(0) == wi.name)
      sps.push_back((ScoreProducer*) gds[i]);
  }
  
  if(wi.ffIndex < sps.size()) {
    
    const ScoreProducer* sp = sps[wi.ffIndex];
    size_t weightStart = StaticData::Instance().GetScoreIndexManager().GetBeginIndex(sp->GetScoreBookkeepingID());
    size_t weightEnd   = StaticData::Instance().GetScoreIndexManager().GetEndIndex(sp->GetScoreBookkeepingID());
    size_t weightSize = weightEnd - weightStart;
    
    while(wi.ffWeightIndex >= weightSize && wi.ffIndex < sps.size()-1) {
      wi.ffIndex++;
      wi.ffWeightIndex -= weightSize;
      
      sp = sps[wi.ffIndex];
      weightStart = StaticData::Instance().GetScoreIndexManager().GetBeginIndex(sp->GetScoreBookkeepingID());
      weightEnd   = StaticData::Instance().GetScoreIndexManager().GetEndIndex(sp->GetScoreBookkeepingID());
      weightSize  = weightEnd - weightStart;
      
      VERBOSE(verbose, "Wrapping around end of weight vector. Setting now weight " << wi.GetDescription()
	      << " for TranslationSystem " << system.GetId() << " to " << wi.value << std::endl);
    }
    
    if(wi.ffWeightIndex < weightSize) {
      float prevValue = m_allWeights[weightStart + wi.ffWeightIndex];
      m_allWeights[weightStart + wi.ffWeightIndex] = wi.value;
      VERBOSE(verbose, "Set weight for (" << sp->GetScoreBookkeepingID() << "," << sp->GetScoreProducerWeightShortName()
          << "," << sp->GetScoreProducerDescription() << "," << wi.ffWeightIndex << ") to " << wi.value << std::endl);
      oldWeight.value = prevValue;
    }
    else {
      VERBOSE(verbose, "Weight index " << wi.ffWeightIndex << " exceeded length of weight vector (" << weightSize << ") for ("
              << sp->GetScoreBookkeepingID() << "," << sp->GetScoreProducerWeightShortName()
              << "," << sp->GetScoreProducerDescription() << "). Nothing set." << std::endl);
      oldWeight.value = MAX_FLOAT;
    }
  }
  else {
    VERBOSE(verbose, "Feature function index " << wi.ffIndex << " exceeded length of feature function vector ("
            << sps.size() << ") for type " << wi.name << ". Nothing set." << std::endl);
    oldWeight.value = MAX_FLOAT;
  }
  
  return oldWeight;
}

// MJD: even more magic!
Parameter StaticData::SetGlobalParameters(const Parameter& parameter)
{
  ResetUserTime();
  
  Parameter prevParameter = *m_parameter;
  m_parameter->Overwrite(parameter);

  // verbose level
  m_verboseLevel = 1;
  if (m_parameter->GetParam("verbose").size() == 1) {
    m_verboseLevel = Scan<size_t>( m_parameter->GetParam("verbose")[0]);
  }

  // to cube or not to cube
  m_searchAlgorithm = (m_parameter->GetParam("search-algorithm").size() > 0) ?
                      (SearchAlgorithm) Scan<size_t>(m_parameter->GetParam("search-algorithm")[0]) : Normal;

  if(m_parameter->GetParam("recover-input-path").size()) {
    m_recoverPath = Scan<bool>(m_parameter->GetParam("recover-input-path")[0]);
    if (m_recoverPath && m_inputType == SentenceInput) {
      TRACE_ERR("--recover-input-path should only be used with confusion net or word lattice input!\n");
      m_recoverPath = false;
    }
  }

  SetBooleanParameter( &m_continuePartialTranslation, "continue-partial-translation", false );

  //word-to-word alignment
  SetBooleanParameter( &m_UseAlignmentInfo, "use-alignment-info", false );
  SetBooleanParameter( &m_PrintAlignmentInfo, "print-alignment-info", false );
  SetBooleanParameter( &m_PrintAlignmentInfoNbest, "print-alignment-info-in-n-best", false );

  SetBooleanParameter( &m_outputHypoScore, "output-hypo-score", false );

  if (!m_UseAlignmentInfo && m_PrintAlignmentInfo) {
    TRACE_ERR("--print-alignment-info should only be used together with \"--use-alignment-info true\". Continue forcing to false.\n");
    m_PrintAlignmentInfo=false;
  }
  if (!m_UseAlignmentInfo && m_PrintAlignmentInfoNbest) {
    TRACE_ERR("--print-alignment-info-in-n-best should only be used together with \"--use-alignment-info true\". Continue forcing to false.\n");
    m_PrintAlignmentInfoNbest=false;
  }

  // n-best
  if (m_parameter->GetParam("n-best-list").size() >= 2) {
    m_nBestFilePath = m_parameter->GetParam("n-best-list")[0];
    m_nBestSize = Scan<size_t>( m_parameter->GetParam("n-best-list")[1] );
    m_onlyDistinctNBest=(m_parameter->GetParam("n-best-list").size()>2 && m_parameter->GetParam("n-best-list")[2]=="distinct");
  } else if (m_parameter->GetParam("n-best-list").size() == 1) {
    UserMessage::Add(string("ERROR: wrong format for switch -n-best-list file size"));
    return prevParameter;
  } else {
    m_nBestSize = 0;
  }
  if (m_parameter->GetParam("n-best-factor").size() > 0) {
    m_nBestFactor = Scan<size_t>( m_parameter->GetParam("n-best-factor")[0]);
  } else {
    m_nBestFactor = 20;
  }

  // include feature names in the n-best list
  SetBooleanParameter( &m_labeledNBestList, "labeled-n-best-list", true );

  // include word alignment in the n-best list
  SetBooleanParameter( &m_nBestIncludesAlignment, "include-alignment-in-n-best", false );

  // printing source phrase spans
  SetBooleanParameter( &m_reportSegmentation, "report-segmentation", false );

  // print all factors of output translations
  SetBooleanParameter( &m_reportAllFactors, "report-all-factors", false );

  // print all factors of output translations
  SetBooleanParameter( &m_reportAllFactorsNBest, "report-all-factors-in-n-best", false );

  //
  if (m_inputType == SentenceInput) {
    SetBooleanParameter( &m_useTransOptCache, "use-persistent-cache", true );
    m_transOptCacheMaxSize = (m_parameter->GetParam("persistent-cache-size").size() > 0)
                             ? Scan<size_t>(m_parameter->GetParam("persistent-cache-size")[0]) : DEFAULT_MAX_TRANS_OPT_CACHE_SIZE;
  } else {
    m_useTransOptCache = false;
  }

  //source word deletion
  SetBooleanParameter( &m_wordDeletionEnabled, "phrase-drop-allowed", false );

  //Disable discarding
  SetBooleanParameter(&m_disableDiscarding, "disable-discarding", false);

  //Print All Derivations
  SetBooleanParameter( &m_printAllDerivations , "print-all-derivations", false );

  // additional output
  if (m_parameter->isParamSpecified("translation-details")) {
    const vector<string> &args = m_parameter->GetParam("translation-details");
    if (args.size() == 1) {
      m_detailedTranslationReportingFilePath = args[0];
    } else {
      UserMessage::Add(string("the translation-details option requires exactly one filename argument"));
      return prevParameter;
    }
  }

  m_maxDistortion = (m_parameter->GetParam("distortion-limit").size() > 0) ?
                    Scan<int>(m_parameter->GetParam("distortion-limit")[0])
                    : -1;
  SetBooleanParameter( &m_reorderingConstraint, "monotone-at-punctuation", false );

  // settings for pruning
  m_maxHypoStackSize = (m_parameter->GetParam("stack").size() > 0)
                       ? Scan<size_t>(m_parameter->GetParam("stack")[0]) : DEFAULT_MAX_HYPOSTACK_SIZE;
  m_minHypoStackDiversity = 0;
  if (m_parameter->GetParam("stack-diversity").size() > 0) {
    if (m_maxDistortion > 15) {
      UserMessage::Add("stack diversity > 0 is not allowed for distortion limits larger than 15");
      return prevParameter;
    }
    if (m_inputType == WordLatticeInput) {
      UserMessage::Add("stack diversity > 0 is not allowed for lattice input");
      return prevParameter;
    }
    m_minHypoStackDiversity = Scan<size_t>(m_parameter->GetParam("stack-diversity")[0]);
  }

  m_beamWidth = (m_parameter->GetParam("beam-threshold").size() > 0) ?
                TransformScore(Scan<float>(m_parameter->GetParam("beam-threshold")[0]))
                : TransformScore(DEFAULT_BEAM_WIDTH);
  m_earlyDiscardingThreshold = (m_parameter->GetParam("early-discarding-threshold").size() > 0) ?
                               TransformScore(Scan<float>(m_parameter->GetParam("early-discarding-threshold")[0]))
                               : TransformScore(DEFAULT_EARLY_DISCARDING_THRESHOLD);
  m_translationOptionThreshold = (m_parameter->GetParam("translation-option-threshold").size() > 0) ?
                                 TransformScore(Scan<float>(m_parameter->GetParam("translation-option-threshold")[0]))
                                 : TransformScore(DEFAULT_TRANSLATION_OPTION_THRESHOLD);

  m_maxNoTransOptPerCoverage = (m_parameter->GetParam("max-trans-opt-per-coverage").size() > 0)
                               ? Scan<size_t>(m_parameter->GetParam("max-trans-opt-per-coverage")[0]) : DEFAULT_MAX_TRANS_OPT_SIZE;

  m_maxNoPartTransOpt = (m_parameter->GetParam("max-partial-trans-opt").size() > 0)
                        ? Scan<size_t>(m_parameter->GetParam("max-partial-trans-opt")[0]) : DEFAULT_MAX_PART_TRANS_OPT_SIZE;

  m_maxPhraseLength = (m_parameter->GetParam("max-phrase-length").size() > 0)
                      ? Scan<size_t>(m_parameter->GetParam("max-phrase-length")[0]) : DEFAULT_MAX_PHRASE_LENGTH;

  m_cubePruningPopLimit = (m_parameter->GetParam("cube-pruning-pop-limit").size() > 0)
                          ? Scan<size_t>(m_parameter->GetParam("cube-pruning-pop-limit")[0]) : DEFAULT_CUBE_PRUNING_POP_LIMIT;

  m_cubePruningDiversity = (m_parameter->GetParam("cube-pruning-diversity").size() > 0)
                           ? Scan<size_t>(m_parameter->GetParam("cube-pruning-diversity")[0]) : DEFAULT_CUBE_PRUNING_DIVERSITY;

  // unknown word processing
  SetBooleanParameter( &m_dropUnknown, "drop-unknown", false );

  // minimum Bayes risk decoding
  SetBooleanParameter( &m_mbr, "minimum-bayes-risk", false );
  m_mbrSize = (m_parameter->GetParam("mbr-size").size() > 0) ?
              Scan<size_t>(m_parameter->GetParam("mbr-size")[0]) : 200;
  m_mbrScale = (m_parameter->GetParam("mbr-scale").size() > 0) ?
               Scan<float>(m_parameter->GetParam("mbr-scale")[0]) : 1.0f;

  //lattice mbr
  SetBooleanParameter( &m_useLatticeMBR, "lminimum-bayes-risk", false );
  if (m_useLatticeMBR && m_mbr) {
    cerr << "Error: Cannot use both n-best mbr and lattice mbr together" << endl;
    exit(1);
  }

  if (m_useLatticeMBR) m_mbr = true;

  m_lmbrPruning = (m_parameter->GetParam("lmbr-pruning-factor").size() > 0) ?
                  Scan<size_t>(m_parameter->GetParam("lmbr-pruning-factor")[0]) : 30;
  m_lmbrThetas = Scan<float>(m_parameter->GetParam("lmbr-thetas"));
  SetBooleanParameter( &m_useLatticeHypSetForLatticeMBR, "lattice-hypo-set", false );
  m_lmbrPrecision = (m_parameter->GetParam("lmbr-p").size() > 0) ?
                    Scan<float>(m_parameter->GetParam("lmbr-p")[0]) : 0.8f;
  m_lmbrPRatio = (m_parameter->GetParam("lmbr-r").size() > 0) ?
                 Scan<float>(m_parameter->GetParam("lmbr-r")[0]) : 0.6f;
  m_lmbrMapWeight = (m_parameter->GetParam("lmbr-map-weight").size() >0) ?
                    Scan<float>(m_parameter->GetParam("lmbr-map-weight")[0]) : 0.0f;

  //consensus decoding
  SetBooleanParameter( &m_useConsensusDecoding, "consensus-decoding", false );
  if (m_useConsensusDecoding && m_mbr) {
    cerr<< "Error: Cannot use consensus decoding together with mbr" << endl;
    exit(1);
  }
  if (m_useConsensusDecoding) m_mbr=true;


  m_timeout_threshold = (m_parameter->GetParam("time-out").size() > 0) ?
                        Scan<size_t>(m_parameter->GetParam("time-out")[0]) : -1;
  m_timeout = (GetTimeoutThreshold() == (size_t)-1) ? false : true;


  m_lmcache_cleanup_threshold = (m_parameter->GetParam("clean-lm-cache").size() > 0) ?
                                Scan<size_t>(m_parameter->GetParam("clean-lm-cache")[0]) : 1;

  // use of xml in input
  if (m_parameter->GetParam("xml-input").size() == 0) m_xmlInputType = XmlPassThrough;
  else if (m_parameter->GetParam("xml-input")[0]=="exclusive") m_xmlInputType = XmlExclusive;
  else if (m_parameter->GetParam("xml-input")[0]=="inclusive") m_xmlInputType = XmlInclusive;
  else if (m_parameter->GetParam("xml-input")[0]=="ignore") m_xmlInputType = XmlIgnore;
  else if (m_parameter->GetParam("xml-input")[0]=="pass-through") m_xmlInputType = XmlPassThrough;
  else {
    UserMessage::Add("invalid xml-input value, must be pass-through, exclusive, inclusive, or ignore");
    return prevParameter;
  }
  
  //configure the translation systems with these tables
  return prevParameter;
}

}


