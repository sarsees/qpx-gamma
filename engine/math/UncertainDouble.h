#ifndef UNCERTAINDOUBLE_H
#define UNCERTAINDOUBLE_H

#include <string>

class UncertainDouble
{
public:
    enum UncertaintyType {
        UndefinedType         = 0x0,
        SymmetricUncertainty  = 0x1,
        AsymmetricUncertainty = 0x2,
        LessThan              = 0x3,
        LessEqual             = 0x4,
        GreaterThan           = 0x5,
        GreaterEqual          = 0x6,
        Approximately         = 0x7,
        Calculated            = 0x8,
        Systematics           = 0x9
    };

    enum Sign {
        UndefinedSign         = 0x0,
        MagnitudeDefined      = 0x1,
        SignDefined           = 0x2,
        SignMagnitudeDefined  = 0x3
    };

    UncertainDouble();
    UncertainDouble(double val, double symmetricSigma, uint16_t sigf, Sign s = SignMagnitudeDefined);

    static UncertainDouble from_int(int64_t val, double sigma);
    static UncertainDouble from_uint(uint64_t val, double sigma);
    static UncertainDouble from_double(double val, double sigma,
                                       uint16_t sigs_below = 2,
                                       Sign s = SignMagnitudeDefined);

    static UncertainDouble from_nsdf(std::string val, std::string uncert);
    static bool is_uncert(std::string str);

    UncertainDouble & operator=(const UncertainDouble & other);

    double value() const;
    double lowerUncertainty() const;
    double upperUncertainty() const;
    UncertaintyType uncertaintyType() const;
    Sign sign() const;
    uint16_t sigfigs() const;
    uint16_t sigdec() const;

    void setValue(double val, Sign s = SignMagnitudeDefined);
    void setUncertainty(double lower, double upper, UncertaintyType type);
    void setSymmetricUncertainty(double sigma);
    void setAsymmetricUncertainty(double lowerSigma, double upperSigma);
    void setSign(Sign s);
    void setSigFigs(uint16_t sig);

    bool hasFiniteValue() const;

    std::string to_string(bool prefix_magn, bool with_uncert = true) const;
    std::string to_markup() const; // outputs formatted text

    UncertainDouble & operator*=(double other);
    UncertainDouble & operator+=(const UncertainDouble &other);
    UncertainDouble operator+(const UncertainDouble &other) const;
    operator double() const;

private:
    double value_;
    double lower_sigma_, upper_sigma_;
    Sign sign_;
    UncertaintyType type_;
    uint16_t sigfigs_;
};

#endif // UNCERTAINDOUBLE_H
