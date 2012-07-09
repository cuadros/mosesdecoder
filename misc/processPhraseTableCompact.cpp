#include <iostream>
#include "TypeDef.h"
#include "CompactPT/PhrasetableCreator.h"
#include "CompactPT/CanonicalHuffman.h"

using namespace Moses;

void printHelp(char **argv) {
  std::cerr << "Usage " << argv[0] << ":\n"
            "options: \n"
            "\t-in  string       -- input table file name\n"
            "\t-out string       -- prefix of binary table file\n"
            "\t-encoding string  -- Encoding type (PREnc REnc None)\n"
            "\t-maxrank int      -- Maximum rank for PREnc\n"
            "\t-nscores int      -- number of score components in phrase table\n"
            "\t-alignment-info   -- include alignment info in the binary phrase table\n"
            "\nadvanced:\n"
            "\t-landmark int     -- use landmark phrase every 2^n source phrases\n"
            "\t-fingerprint int  -- number of bits used for source phrase fingerprints\n"
            "\t-join-scores      -- single set of Huffman codes for score components\n"
            "\t-quantize int     -- maximum number of scores per score component\n"

#ifdef WITH_THREADS
            "\t-threads int      -- number of threads used for conversion\n"
#endif 
            "\n";
}


int main(int argc, char **argv) {
  std::cerr << "processPhraseTableCompact by Marcin Junczys-Dowmunt\n";
    
  std::string inFilePath;
  std::string outFilePath("out");
  PhrasetableCreator::Coding coding = PhrasetableCreator::PREnc;
  
  size_t numScoreComponent = 5;  
  size_t orderBits = 10;
  size_t fingerprintBits = 16;
  bool useAlignmentInfo = false;
  bool multipleScoreTrees = true;
  size_t quantize = 0;
  size_t maxRank = 100;
  size_t threads = 1;
  
  if(1 >= argc) {
    printHelp(argv);
    return 1;
  }
  for(int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if("-in" == arg && i+1 < argc) {
      ++i;
      inFilePath = argv[i];
    }
    else if("-out" == arg && i+1 < argc) {
      ++i;
      outFilePath = argv[i];
    }
    else if("-encoding" == arg && i+1 < argc) {
      ++i;
      std::string val(argv[i]);
      if(val == "None" || val == "none") {
        coding = PhrasetableCreator::None;
      }
      else if(val == "REnc" || val == "renc") {
        coding = PhrasetableCreator::REnc;
      }
      else if(val == "PREnc" || val == "prenc") {
        coding = PhrasetableCreator::PREnc;
      }
    }
    else if("-maxrank" == arg && i+1 < argc) {
      ++i;
      maxRank = atoi(argv[i]);
    }
    else if("-nscores" == arg && i+1 < argc) {
      ++i;
      numScoreComponent = atoi(argv[i]);
    }
    else if("-alignment-info" == arg) {
      useAlignmentInfo = true;
    }
    else if("-landmark" == arg && i+1 < argc) {
      ++i;
      orderBits = atoi(argv[i]);
    }
    else if("-fingerprint" == arg && i+1 < argc) {
      ++i;
      fingerprintBits = atoi(argv[i]);
    }
    else if("-join-scores" == arg) {
      multipleScoreTrees = false;
    }
    else if("-quantize" == arg && i+1 < argc) {
      ++i;
      quantize = atoi(argv[i]);
    }
    else if("-threads" == arg && i+1 < argc) {
#ifdef WITH_THREADS
      ++i;
      threads = atoi(argv[i]);
#else
      std::cerr << "Thread support not compiled in" << std::endl;
      exit(1);
#endif
    }
    else {
      //somethings wrong... print help
      printHelp(argv);
      return 1;
    }
  }
  
  PhrasetableCreator(inFilePath, outFilePath, numScoreComponent,
                     coding, orderBits, fingerprintBits,
                     useAlignmentInfo, multipleScoreTrees,
                     quantize, maxRank
#ifdef WITH_THREADS
                     , threads
#endif                     
                     );
}
