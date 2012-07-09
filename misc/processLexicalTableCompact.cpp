#include <iostream>
#include <string>

#include "Timer.h"
#include "InputFileStream.h"
#include "LexicalReorderingTable.h"

using namespace Moses;

Timer timer;

void printHelp()
{
  std::cerr << "Usage:\n"
            "options: \n"
            "\t-in  string -- input table file name\n"
            "\t-out string -- prefix of binary table file\n"
            "\n";
}

int main(int argc, char** argv)
{
  std::cerr << "processLexicalTableHashed by Marcin Junczys-Dowmunt\n";
  std::string inFilePath;
  std::string outFilePath("out");
  if(1 >= argc) {
    printHelp();
    return 1;
  }
  for(int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if("-in" == arg && i+1 < argc) {
      ++i;
      inFilePath = argv[i];
    } else if("-out" == arg && i+1 < argc) {
      ++i;
      outFilePath = argv[i];
    } else {
      //somethings wrong... print help
      printHelp();
      return 1;
    }
  }


  std::cerr << "Processing " << inFilePath<< " to " << outFilePath << std::endl;
  std::vector<FactorType> e_factors(1, 0);
  std::vector<FactorType> f_factors(1, 0);
  std::vector<FactorType> c_factors;
  LexicalReorderingTableMemoryHashed lextable(e_factors, f_factors, c_factors);
  lextable.LoadText(inFilePath);
  std::cerr << "Saving now to " << outFilePath << ".mphlexr" << std::endl;
  lextable.SaveBinary(outFilePath + ".mphlexr");
  std::cerr << "Done" << std::endl;
}
