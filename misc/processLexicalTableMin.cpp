#include <iostream>
#include <string>

#include "Timer.h"
#include "InputFileStream.h"
#include "CompactPT/LexicalReorderingTableCreator.h"

using namespace Moses;

Timer timer;

void printHelp(char **argv)
{
  std::cerr << "Usage " << argv[0] << ":\n"
            "options: \n"
            "\t-in  string       -- input table file name\n"
            "\t-out string       -- prefix of binary table file\n"
#ifdef WITH_THREADS
            "\t-threads int      -- number of threads used for conversion\n"
#endif 
            "\nadvanced:\n"
            "\t-landmark int     -- use landmark phrase every 2^n phrases\n"
            "\t-fingerprint int  -- number of bits used for phrase fingerprints\n"
            "\t-join-scores      -- single set of Huffman codes for score components\n"
            "\t-quantize int     -- maximum number of scores per score component\n"
            "\n"
            
            "  For more information see: http://www.statmt.org/moses/?n=Moses.AdvancedFeatures#ntoc6\n"
            "  and\n\n"
            "  @article { junczys_pbml98_2012,\n"
            "      author = { Marcin Junczys-Dowmunt },\n"
            "      title = { Phrasal Rank-Encoding: Exploiting Phrase Redundancy and\n"
            "                Translational Relations for Phrase Table Compression },\n"
            "      journal = { The Prague Bulletin of Mathematical Linguistics },\n"
            "      volume = { 98 },\n"
            "      year = { 2012 },\n"
            "      note = { Proceedings of the MT Marathon 2012, Edinburgh },\n"
            "  }\n\n";
}

int main(int argc, char** argv)
{
  
  std::string inFilePath;
  std::string outFilePath("out");
  
  size_t numScoreComponent = 6;
  size_t orderBits = 10;
  size_t fingerPrintBits = 16;
  bool multipleScoreTrees = true;
  size_t quantize = 0;

#ifdef WITH_THREADS
  size_t threads = 2;
#endif   

  if(1 >= argc)
  {
    printHelp(argv);
    return 1;
  }
  for(int i = 1; i < argc; ++i)
  {
    std::string arg(argv[i]);
    if("-in" == arg && i+1 < argc)
    {
      ++i;
      inFilePath = argv[i];
    }
    else if("-out" == arg && i+1 < argc)
    {
      ++i;
      outFilePath = argv[i];
    }
    else if("-landmark" == arg && i+1 < argc)
    {
      ++i;
      orderBits = atoi(argv[i]);
    }
    else if("-fingerprint" == arg && i+1 < argc)
    {
      ++i;
      fingerPrintBits = atoi(argv[i]);
    }
    else if("-join-scores" == arg)
    {
      multipleScoreTrees = false;
    }
    else if("-quantize" == arg && i+1 < argc)
    {
      ++i;
      quantize = atoi(argv[i]);
    }
    else if("-threads" == arg && i+1 < argc)
    {
#ifdef WITH_THREADS
      ++i;
      threads = atoi(argv[i]);
#else
      std::cerr << "Thread support not compiled in" << std::endl;
      exit(1);
#endif
    }
    else
    {
      //somethings wrong... print help
      printHelp(argv);
      return 1;
    }
  }
  
  if(outFilePath.rfind(".minlexr") != outFilePath.size() - 8)
    outFilePath += ".minlexr";

  LexicalReorderingTableCreator(
    inFilePath, outFilePath, numScoreComponent,
    orderBits, fingerPrintBits,
    multipleScoreTrees, quantize
#ifdef WITH_THREADS
    , threads
#endif   
  );
}
