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

#include "DecodeStepTranslation.h"
#include "PhraseDictionary.h"
#include "TranslationOption.h"
#include "TranslationOptionCollection.h"
#include "PartialTranslOptColl.h"
#include "FactorCollection.h"

DecodeStepTranslation::DecodeStepTranslation(PhraseDictionaryBase* dict, const DecodeStep* prev)
: DecodeStep(dict, prev)
{
}

const PhraseDictionaryBase &DecodeStepTranslation::GetPhraseDictionary() const
{
  return *static_cast<const PhraseDictionaryBase*>(m_ptr);
}

TranslationOption *DecodeStepTranslation::MergeTranslation(const TranslationOption& oldTO, const TargetPhrase &targetPhrase) const
{
  if (IsFilteringStep()) {
    if (!oldTO.IsCompatible(targetPhrase, m_conflictFactors)) return 0;
  }

  TranslationOption *newTransOpt = new TranslationOption(oldTO);
  newTransOpt->MergeNewFeatures(targetPhrase, targetPhrase.GetScoreBreakdown(), m_newOutputFactors);
  return newTransOpt;
}


void DecodeStepTranslation::Process(const TranslationOption &inputPartialTranslOpt
                              , const DecodeStep &decodeStep
                              , PartialTranslOptColl &outputPartialTranslOptColl
                              , FactorCollection &factorCollection
                              , TranslationOptionCollection *toc
                              , bool observeTableLimit) const
{
  //TRACE_ERR(inputPartialTranslOpt << endl);
  if (inputPartialTranslOpt.GetTargetPhrase().GetSize() == 0)
    { // word deletion

      outputPartialTranslOptColl.Add(new TranslationOption(inputPartialTranslOpt));

      return;
    }

  // normal trans step
  const WordsRange &sourceWordsRange        = inputPartialTranslOpt.GetSourceWordsRange();
  const PhraseDictionaryBase &phraseDictionary  = decodeStep.GetPhraseDictionary();
	const size_t currSize = inputPartialTranslOpt.GetTargetPhrase().GetSize();
	const size_t tableLimit = phraseDictionary.GetTableLimit();
	
  const TargetPhraseCollection *phraseColl= phraseDictionary.GetTargetPhraseCollection(toc->GetSource(),sourceWordsRange);

  if (phraseColl != NULL)
    {
      TargetPhraseCollection::const_iterator iterTargetPhrase, iterEnd;
		 	iterEnd = (!observeTableLimit || tableLimit == 0 || phraseColl->GetSize() < tableLimit) ? phraseColl->end() : phraseColl->begin() + tableLimit;
			
      for (iterTargetPhrase = phraseColl->begin(); iterTargetPhrase != iterEnd; ++iterTargetPhrase)
        {
          const TargetPhrase& targetPhrase = **iterTargetPhrase;
					// skip if the 
					if (targetPhrase.GetSize() != currSize) continue;

          TranslationOption *newTransOpt = MergeTranslation(inputPartialTranslOpt, targetPhrase);
          if (newTransOpt != NULL)
            {
              outputPartialTranslOptColl.Add( newTransOpt );
            }
        }
    }
  else if (sourceWordsRange.GetWordsCount() == 1)
    { // unknown handler
      //toc->ProcessUnknownWord(sourceWordsRange.GetStartPos(), factorCollection);
    }
}
