

#pragma once

#include <string>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <sstream>
#include <vector>

namespace validation {

class ValidationError : public std::runtime_error {
public:
    explicit ValidationError(const std::string& message) 
        : std::runtime_error(message) {}
};

class RangeError : public ValidationError {
public:
    RangeError(const std::string& paramName, double value, double min, double max)
        : ValidationError(formatRangeError(paramName, value, min, max)),
          paramName_(paramName), value_(value), min_(min), max_(max) {}
    
    const std::string& paramName() const { return paramName_; }
    double value() const { return value_; }
    double min() const { return min_; }
    double max() const { return max_; }
    
private:
    static std::string formatRangeError(const std::string& name, double val, double min, double max) {
        std::ostringstream oss;
        oss << "RangeError: '" << name << "' = " << val 
            << " is outside valid range [" << min << ", " << max << "]";
        return oss.str();
    }
    
    std::string paramName_;
    double value_, min_, max_;
};

class PositivityError : public ValidationError {
public:
    PositivityError(const std::string& paramName, double value)
        : ValidationError(formatPositivityError(paramName, value)),
          paramName_(paramName), value_(value) {}
    
private:
    static std::string formatPositivityError(const std::string& name, double val) {
        std::ostringstream oss;
        oss << "PositivityError: '" << name << "' = " << val 
            << " must be strictly positive (> 0)";
        return oss.str();
    }
    
    std::string paramName_;
    double value_;
};

class NaNInfError : public ValidationError {
public:
    NaNInfError(const std::string& paramName, double value)
        : ValidationError(formatNaNInfError(paramName, value)),
          paramName_(paramName), value_(value) {}
    
private:
    static std::string formatNaNInfError(const std::string& name, double val) {
        std::ostringstream oss;
        oss << "NaNInfError: '" << name << "' = " << val << " is not a finite number";
        return oss.str();
    }
    
    std::string paramName_;
    double value_;
};

class EmptyContainerError : public ValidationError {
public:
    explicit EmptyContainerError(const std::string& containerName)
        : ValidationError("EmptyContainerError: '" + containerName + "' must not be empty") {}
};

class MinimumSizeError : public ValidationError {
public:
    MinimumSizeError(const std::string& containerName, size_t actual, size_t required)
        : ValidationError(formatMinSizeError(containerName, actual, required)) {}
    
private:
    static std::string formatMinSizeError(const std::string& name, size_t actual, size_t req) {
        std::ostringstream oss;
        oss << "MinimumSizeError: '" << name << "' has " << actual 
            << " elements, but requires at least " << req;
        return oss.str();
    }
};

class NullPointerError : public ValidationError {
public:
    explicit NullPointerError(const std::string& ptrName)
        : ValidationError("NullPointerError: '" + ptrName + "' must not be null") {}
};

class AllocationError : public ValidationError {
public:
    explicit AllocationError(const std::string& context, size_t bytes = 0)
        : ValidationError(formatAllocationError(context, bytes)) {}
    
private:
    static std::string formatAllocationError(const std::string& ctx, size_t bytes) {
        std::ostringstream oss;
        oss << "AllocationError: Failed to allocate memory for '" << ctx << "'";
        if (bytes > 0) {
            oss << " (" << bytes << " bytes)";
        }
        return oss.str();
    }
};

class FileIOError : public ValidationError {
public:
    FileIOError(const std::string& operation, const std::string& path)
        : ValidationError("FileIOError: " + operation + " failed for '" + path + "'") {}
};

class ParseError : public ValidationError {
public:
    ParseError(const std::string& what, const std::string& value)
        : ValidationError("ParseError: Failed to parse " + what + " from '" + value + "'") {}
};

class Validator {
public:

    static void requireFinite(const std::string& paramName, double value) {
        if (!std::isfinite(value)) {
            throw NaNInfError(paramName, value);
        }
    }

    static void requirePositive(const std::string& paramName, double value) {
        requireFinite(paramName, value);
        if (value <= 0.0) {
            throw PositivityError(paramName, value);
        }
    }

    static void requireNonNegative(const std::string& paramName, double value) {
        requireFinite(paramName, value);
        if (value < 0.0) {
            throw RangeError(paramName, value, 0.0, std::numeric_limits<double>::max());
        }
    }

    static void requireInRange(const std::string& paramName, double value, 
                               double min, double max) {
        requireFinite(paramName, value);
        if (value < min || value > max) {
            throw RangeError(paramName, value, min, max);
        }
    }

    static void requireInRangeExclusive(const std::string& paramName, double value,
                                        double min, double max) {
        requireFinite(paramName, value);
        if (value <= min || value >= max) {
            throw RangeError(paramName, value, min, max);
        }
    }

    static void requireValidSpot(double spot) {
        requirePositive("spot", spot);
    }

    static void requireValidStrike(double strike) {
        requirePositive("strike", strike);
    }

    static void requireValidExpiry(double expiry) {
        requirePositive("expiry", expiry);
    }

    static void requireValidIV(double iv, double maxIV = 5.0) {
        requireFinite("implied_volatility", iv);
        if (iv <= 0.0 || iv > maxIV) {
            throw RangeError("implied_volatility", iv, 0.0, maxIV);
        }
    }

    static void requireValidRiskFreeRate(double rate) {
        requireInRange("risk_free_rate", rate, -0.5, 1.0);
    }

    static void requireValidDividendYield(double yield) {
        requireInRange("dividend_yield", yield, 0.0, 0.5);
    }

    static void requireValidCorrelation(double rho) {
        requireInRangeExclusive("correlation", rho, -1.0, 1.0);
    }

    template<typename Container>
    static void requireNonEmpty(const std::string& containerName, const Container& c) {
        if (c.empty()) {
            throw EmptyContainerError(containerName);
        }
    }
    
    template<typename Container>
    static void requireMinimumSize(const std::string& containerName, 
                                   const Container& c, size_t minSize) {
        if (c.size() < minSize) {
            throw MinimumSizeError(containerName, c.size(), minSize);
        }
    }

    template<typename T>
    static void requireNonNull(const std::string& ptrName, const T* ptr) {
        if (ptr == nullptr) {
            throw NullPointerError(ptrName);
        }
    }

    template<typename T>
    static T* requireAllocation(T* ptr, const std::string& context, size_t bytes = 0) {
        if (ptr == nullptr) {
            throw AllocationError(context, bytes);
        }
        return ptr;
    }

    struct BSInputs {
        double spot;
        double strike;
        double expiry;
        double sigma;
        double rate;
        double dividendYield;
    };
    
    static void requireValidBSInputs(const BSInputs& inputs) {
        requireValidSpot(inputs.spot);
        requireValidStrike(inputs.strike);
        requireValidExpiry(inputs.expiry);
        requireValidIV(inputs.sigma);
        requireValidRiskFreeRate(inputs.rate);
        requireValidDividendYield(inputs.dividendYield);
    }

    static bool isFinite(double value) { return std::isfinite(value); }
    static bool isPositive(double value) { return std::isfinite(value) && value > 0.0; }
    static bool isNonNegative(double value) { return std::isfinite(value) && value >= 0.0; }
    static bool isInRange(double value, double min, double max) {
        return std::isfinite(value) && value >= min && value <= max;
    }

    static double parseDouble(const std::string& value, const std::string& fieldName) {
        try {
            size_t pos = 0;
            double result = std::stod(value, &pos);
            
            if (pos != value.size()) {
                throw ParseError(fieldName, value);
            }
            return result;
        } catch (const std::invalid_argument&) {
            throw ParseError(fieldName, value);
        } catch (const std::out_of_range&) {
            throw ParseError(fieldName + " (out of range)", value);
        }
    }

    static int parseInt(const std::string& value, const std::string& fieldName) {
        try {
            size_t pos = 0;
            int result = std::stoi(value, &pos);
            if (pos != value.size()) {
                throw ParseError(fieldName, value);
            }
            return result;
        } catch (const std::invalid_argument&) {
            throw ParseError(fieldName, value);
        } catch (const std::out_of_range&) {
            throw ParseError(fieldName + " (out of range)", value);
        }
    }
};

class ValidationResult {
public:
    ValidationResult() = default;
    
    void addError(const std::string& error) {
        errors_.push_back(error);
    }
    
    void addWarning(const std::string& warning) {
        warnings_.push_back(warning);
    }
    
    bool isValid() const { return errors_.empty(); }
    bool hasWarnings() const { return !warnings_.empty(); }
    
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    
    std::string formatErrors(const std::string& separator = "\n") const {
        std::ostringstream oss;
        for (size_t i = 0; i < errors_.size(); ++i) {
            if (i > 0) oss << separator;
            oss << errors_[i];
        }
        return oss.str();
    }
    
    void throwIfInvalid() const {
        if (!isValid()) {
            throw ValidationError("Validation failed:\n" + formatErrors("\n  "));
        }
    }
    
private:
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};

#define VOL_VALIDATE_POSITIVE(var, name) \
    validation::Validator::requirePositive(name, var)

#define VOL_VALIDATE_RANGE(var, name, min, max) \
    validation::Validator::requireInRange(name, var, min, max)

#define VOL_VALIDATE_FINITE(var, name) \
    validation::Validator::requireFinite(name, var)

#define VOL_SAFE_MALLOC(type, ptr, size) \
    validation::Validator::requireAllocation<type>( \
        static_cast<type*>(malloc(size)), #ptr, size)

} 
