#include "Parser.hpp"
#include "Cycript.tab.hh"

CYRange DigitRange_    (0x3ff000000000000LLU, 0x000000000000000LLU); // 0-9
CYRange WordStartRange_(0x000001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$
CYRange WordEndRange_  (0x3ff001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$0-9

CYDriver::CYDriver(const std::string &filename) :
    state_(CYClear),
    data_(NULL),
    size_(0),
    file_(NULL),
    strict_(false),
    filename_(filename),
    program_(NULL)
{
    ScannerInit();
}

CYDriver::~CYDriver() {
    ScannerDestroy();
}

void CYDriver::Warning(const cy::location &location, const char *message) {
    if (!strict_)
        return;

    CYDriver::Error error;
    error.warning_ = true;
    error.location_ = location;
    error.message_ = message;
    errors_.push_back(error);
}

void cy::parser::error(const cy::parser::location_type &location, const std::string &message) {
    CYDriver::Error error;
    error.warning_ = false;
    error.location_ = location;
    error.message_ = message;
    driver.errors_.push_back(error);
}
