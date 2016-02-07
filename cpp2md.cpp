/****

cpp2md is a simple utility that converts a C++ source file to a Markdown file.

The rules it follows to do this are simple:

- Text contained in lines between `/ ****` and `**** /` are Markdown-format comments, and should be passed through unaltered to the Markdown output file.
- All other lines are C++, and should be indented four spaces so they are treated as code blocks.

The `/ ****` and `**** /` tokens must be at the initial position on the line.

Note: The comment markers above are displayed above with a space between the
`/` and `****`.  You must not include that space when writing comments, but we
have to do it here so that the C++ compiler won't think we are trying to nest
such comments within this comment block.

****/

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

void convertToMarkdown(std::istream& input, std::ostream& output) {
    bool inCPlusPlusSection = false;

    std::string line;
    while (std::getline(input, line)) {
        if (line.find("/****") == 0) {
            inCPlusPlusSection = false;
        }
        else if (line.find("****/") == 0) {
            inCPlusPlusSection = true;
        }
        else {
            if (inCPlusPlusSection)
                output << "    ";
            output << line << "\n";
        }   
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
        
        std::ifstream infile(inpath);
        if (!infile.is_open()) {
            std::cerr << "cpp2md: unable to open input file \"" << inpath << "\"" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::ofstream outfile(outpath);
        if (!outfile.is_open()) {
            std::cerr << "cpp2md: unable to open output file \"" << outpath << "\"" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        convertToMarkdown(infile, outfile);

        infile.close();
        outfile.close();

        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "cpp2md: " << ex.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

