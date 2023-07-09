#include <exception>
#include <string>
#include <yaml-cpp/yaml.h>

namespace athes::detfm::match {

class ParserError : public std::exception {
public:
    std::string message;
    std::string filename;
    YAML::Mark mark;

    ParserError(std::string message, std::string filename, YAML::Mark mark);

    const char* what() const noexcept override;
};

}