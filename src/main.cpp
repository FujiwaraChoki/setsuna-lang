#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

#include "lexer.hpp"
#include "parser.hpp"
#include "evaluator.hpp"
#include "typechecker.hpp"
#include "environment.hpp"

using namespace setsuna;
namespace fs = std::filesystem;

void printUsage(const char* progname) {
    std::cout << "Setsuna Programming Language v0.1.0\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << progname << "              Start REPL\n";
    std::cout << "  " << progname << " <file.stsn>  Run a file\n";
    std::cout << "  " << progname << " --help       Show this message\n";
}

// Find the prelude file path
std::string findPreludePath() {
    // Try relative paths first (for development)
    std::vector<std::string> searchPaths = {
        "stdlib/prelude.stsn",
        "../stdlib/prelude.stsn",
        "../../stdlib/prelude.stsn",
        // System-wide install locations
        "/usr/local/share/setsuna/prelude.stsn",
        "/usr/share/setsuna/prelude.stsn"
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            return path;
        }
    }

    // Not found
    return "";
}

// Load the prelude file into the environment
void loadPrelude(EnvPtr env) {
    std::string preludePath = findPreludePath();
    if (preludePath.empty()) {
        // Prelude not found - this is acceptable, continue without it
        return;
    }

    std::ifstream file(preludePath);
    if (!file) {
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        Lexer lexer(source, preludePath);
        auto tokens = lexer.tokenize();

        Parser parser(tokens);
        auto program = parser.parse();

        Evaluator evaluator(env);
        evaluator.eval(program);
    } catch (const SetsunaError& e) {
        std::cerr << "Warning: Failed to load prelude: " << e.format() << std::endl;
    }
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void runFile(const std::string& path) {
    std::string source = readFile(path);

    Lexer lexer(source, path);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    auto program = parser.parse();

    auto env = makeGlobalEnv();
    loadPrelude(env);  // Load standard library prelude
    Evaluator evaluator(env);

    auto result = evaluator.eval(program);

    // If the result is not unit, print it
    if (!result->isUnit()) {
        std::cout << result->toString() << std::endl;
    }
}

void repl() {
    std::cout << "Setsuna v0.1.0 - Functional Programming Language\n";
    std::cout << "Type expressions to evaluate. Type 'exit' or Ctrl+D to quit.\n\n";

    auto env = makeGlobalEnv();
    loadPrelude(env);  // Load standard library prelude
    Evaluator evaluator(env);

    std::string line;
    std::string buffer;
    int openBraces = 0;
    int openBrackets = 0;
    int openParens = 0;

    while (true) {
        // Print prompt
        if (buffer.empty()) {
            std::cout << ">> ";
        } else {
            std::cout << ".. ";
        }
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            std::cout << "\nGoodbye!\n";
            break;
        }

        if (buffer.empty() && line == "exit") {
            std::cout << "Goodbye!\n";
            break;
        }

        // Count braces to handle multi-line input
        for (char c : line) {
            if (c == '{') openBraces++;
            else if (c == '}') openBraces--;
            else if (c == '[') openBrackets++;
            else if (c == ']') openBrackets--;
            else if (c == '(') openParens++;
            else if (c == ')') openParens--;
        }

        buffer += line + "\n";

        // If we have balanced delimiters, try to evaluate
        if (openBraces <= 0 && openBrackets <= 0 && openParens <= 0) {
            openBraces = openBrackets = openParens = 0;

            try {
                Lexer lexer(buffer, "<repl>");
                auto tokens = lexer.tokenize();

                Parser parser(tokens);
                auto program = parser.parse();

                auto result = evaluator.eval(program);

                if (!result->isUnit()) {
                    std::cout << "=> " << result->toString() << std::endl;
                }
            } catch (const SetsunaError& e) {
                std::cerr << e.format() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }

            buffer.clear();
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            repl();
        } else if (argc == 2) {
            std::string arg = argv[1];
            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
            } else {
                runFile(arg);
            }
        } else {
            printUsage(argv[0]);
            return 1;
        }
    } catch (const SetsunaError& e) {
        std::cerr << e.format() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
