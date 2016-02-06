#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

void convertToMarkdown(std::istream& input, std::ostream& output) {
    bool inCPlusPlusSection = false;

    std::string line;
    while (std::getline(input, line)) {
        if (line.find("/****") == 0) {
            if (inCPlusPlusSection) {
                output << "'''\n";
            }
        }
        else if (line.find("****/") == 0) {
            inCPlusPlusSection = true;
            output << "'''C++\n";
        }
        else {
            output << line << "\n";
        }   
    }

    if (inCPlusPlusSection) {
        output << "'''\n";
    }
}

void showUsage() {
    std::cout << "Usage: cpp2md INPUTFILE OUTPUTFILE\n"
              << "\n"
              << "  INPUTFILE: path of C++ input file\n"
              << "  OUTPUTFILE: path of Markdown output file\n"
              << "\n"
              << "  Example: cpp2md cxxforth.cpp cxxforth.md\n"
              << std::endl;
}

} // end anonymous namespace

int main(int argc, const char** argv) {
    try {
        if (argc != 3) {
            showUsage();
            std::exit(EXIT_FAILURE);
        }

        auto inpath = std::string(argv[1]);
        auto outpath = std::string(argv[2]);
        
        auto infile = std::ifstream(inpath);
        if (!infile.is_open()) {
            std::cerr << "cpp2md: unable to open input file \"" << inpath << "\"" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        auto outfile = std::ofstream(outpath);
        if (!outfile.is_open()) {
            std::cerr << "cpp2md: unable to open output file \"" << outpath[2] << "\"" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        convertToMarkdown(infile, outfile);

        infile.close();
        outfile.close();

        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "libcpp: " << ex.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

