#pragma once

// =============================================================================
// Math builtins — floor, ceil, round, abs, mod, pow, sqrt, log, log10,
//                 sin, cos, tan, clamp, random, random_int, random_choice,
//                 is_nan, is_inf, to_int, to_float, hex, bin,
//                 PI, E, INF
// =============================================================================

#include "builtin_registry.hpp"
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <bitset>

namespace xell
{

    inline void registerMathBuiltins(BuiltinTable &t)
    {
        t["floor"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("floor", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("floor() expects a number", line);
            return XObject::makeInt(static_cast<int64_t>(std::floor(args[0].asNumber())));
        };

        t["ceil"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ceil", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("ceil() expects a number", line);
            return XObject::makeInt(static_cast<int64_t>(std::ceil(args[0].asNumber())));
        };

        // round(x) or round(x, n) — round to n decimal places
        t["round"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("round", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("round() expects a number", line);
            if (args.size() == 1)
                return XObject::makeInt(static_cast<int64_t>(std::round(args[0].asNumber())));
            // round(x, n) — round to n decimal places
            if (!args[1].isNumber())
                throw TypeError("round() precision must be a number", line);
            int n = (int)args[1].asNumber();
            double factor = std::pow(10.0, n);
            return XObject::makeFloat(std::round(args[0].asNumber() * factor) / factor);
        };

        t["abs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("abs", 1, (int)args.size(), line);
            if (args[0].isInt())
            {
                int64_t val = args[0].asInt();
                return XObject::makeInt(val < 0 ? -val : val);
            }
            if (args[0].isFloat())
                return XObject::makeFloat(std::fabs(args[0].asFloat()));
            if (args[0].isComplex())
                return XObject::makeFloat(args[0].asComplex().magnitude());
            throw TypeError("abs() expects a number", line);
        };

        t["mod"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("mod", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("mod() expects two numbers", line);
            if (args[0].isInt() && args[1].isInt())
            {
                int64_t b = args[1].asInt();
                if (b == 0)
                    throw DivisionByZeroError(line);
                return XObject::makeInt(args[0].asInt() % b);
            }
            double b = args[1].asNumber();
            if (b == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(std::fmod(args[0].asNumber(), b));
        };

        t["pow"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("pow", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("pow() expects two numbers", line);
            // If both are int and exponent >= 0, return int
            if (args[0].isInt() && args[1].isInt() && args[1].asInt() >= 0)
            {
                int64_t base = args[0].asInt(), exp = args[1].asInt();
                int64_t result = 1;
                for (int64_t i = 0; i < exp; ++i)
                    result *= base;
                return XObject::makeInt(result);
            }
            return XObject::makeFloat(std::pow(args[0].asNumber(), args[1].asNumber()));
        };

        t["sqrt"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sqrt", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sqrt() expects a number", line);
            double val = args[0].asNumber();
            if (val < 0.0)
                return XObject::makeComplex(0.0, std::sqrt(-val));
            return XObject::makeFloat(std::sqrt(val));
        };

        // log(x) — natural logarithm
        t["log"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("log", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("log() expects a number", line);
            double val = args[0].asNumber();
            if (val <= 0.0)
                throw TypeError("log() domain error: argument must be positive", line);
            return XObject::makeFloat(std::log(val));
        };

        // log10(x) — base-10 logarithm
        t["log10"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("log10", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("log10() expects a number", line);
            double val = args[0].asNumber();
            if (val <= 0.0)
                throw TypeError("log10() domain error: argument must be positive", line);
            return XObject::makeFloat(std::log10(val));
        };

        // sin(x) — sine in radians
        t["sin"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sin", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sin() expects a number", line);
            return XObject::makeFloat(std::sin(args[0].asNumber()));
        };

        // cos(x) — cosine in radians
        t["cos"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("cos", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("cos() expects a number", line);
            return XObject::makeFloat(std::cos(args[0].asNumber()));
        };

        // tan(x) — tangent in radians
        t["tan"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("tan", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("tan() expects a number", line);
            return XObject::makeFloat(std::tan(args[0].asNumber()));
        };

        // ---- Reciprocal trig ----

        // cot(x) — cotangent = 1/tan(x)
        t["cot"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("cot", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("cot() expects a number", line);
            double tanVal = std::tan(args[0].asNumber());
            if (tanVal == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(1.0 / tanVal);
        };

        // sec(x) — secant = 1/cos(x)
        t["sec"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sec", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sec() expects a number", line);
            double cosVal = std::cos(args[0].asNumber());
            if (cosVal == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(1.0 / cosVal);
        };

        // csc(x) — cosecant = 1/sin(x)
        t["csc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("csc", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("csc() expects a number", line);
            double sinVal = std::sin(args[0].asNumber());
            if (sinVal == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(1.0 / sinVal);
        };

        // ---- Inverse trig ----

        // asin(x) — arc sine
        t["asin"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("asin", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("asin() expects a number", line);
            double val = args[0].asNumber();
            if (val < -1.0 || val > 1.0)
                throw TypeError("asin() domain error: argument must be in [-1, 1]", line);
            return XObject::makeFloat(std::asin(val));
        };

        // acos(x) — arc cosine
        t["acos"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acos", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acos() expects a number", line);
            double val = args[0].asNumber();
            if (val < -1.0 || val > 1.0)
                throw TypeError("acos() domain error: argument must be in [-1, 1]", line);
            return XObject::makeFloat(std::acos(val));
        };

        // atan(x) — arc tangent
        t["atan"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("atan", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("atan() expects a number", line);
            return XObject::makeFloat(std::atan(args[0].asNumber()));
        };

        // atan2(y, x) — two-argument arc tangent
        t["atan2"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("atan2", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("atan2() expects two numbers", line);
            return XObject::makeFloat(std::atan2(args[0].asNumber(), args[1].asNumber()));
        };

        // acot(x) — arc cotangent = atan(1/x)
        t["acot"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acot", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acot() expects a number", line);
            double val = args[0].asNumber();
            if (val == 0.0)
                return XObject::makeFloat(std::atan(1.0) * 2.0); // PI/2
            return XObject::makeFloat(std::atan(1.0 / val));
        };

        // asec(x) — arc secant = acos(1/x)
        t["asec"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("asec", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("asec() expects a number", line);
            double val = args[0].asNumber();
            if (val > -1.0 && val < 1.0)
                throw TypeError("asec() domain error: |x| must be >= 1", line);
            return XObject::makeFloat(std::acos(1.0 / val));
        };

        // acsc(x) — arc cosecant = asin(1/x)
        t["acsc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acsc", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acsc() expects a number", line);
            double val = args[0].asNumber();
            if (val > -1.0 && val < 1.0)
                throw TypeError("acsc() domain error: |x| must be >= 1", line);
            return XObject::makeFloat(std::asin(1.0 / val));
        };

        // ---- Hyperbolic ----

        // sinh(x) — hyperbolic sine
        t["sinh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sinh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sinh() expects a number", line);
            return XObject::makeFloat(std::sinh(args[0].asNumber()));
        };

        // cosh(x) — hyperbolic cosine
        t["cosh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("cosh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("cosh() expects a number", line);
            return XObject::makeFloat(std::cosh(args[0].asNumber()));
        };

        // tanh(x) — hyperbolic tangent
        t["tanh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("tanh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("tanh() expects a number", line);
            return XObject::makeFloat(std::tanh(args[0].asNumber()));
        };

        // coth(x) — hyperbolic cotangent = 1/tanh(x)
        t["coth"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("coth", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("coth() expects a number", line);
            double tanhVal = std::tanh(args[0].asNumber());
            if (tanhVal == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(1.0 / tanhVal);
        };

        // sech(x) — hyperbolic secant = 1/cosh(x)
        t["sech"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sech", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sech() expects a number", line);
            return XObject::makeFloat(1.0 / std::cosh(args[0].asNumber()));
        };

        // csch(x) — hyperbolic cosecant = 1/sinh(x)
        t["csch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("csch", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("csch() expects a number", line);
            double sinhVal = std::sinh(args[0].asNumber());
            if (sinhVal == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(1.0 / sinhVal);
        };

        // ---- Inverse hyperbolic ----

        // asinh(x) — inverse hyperbolic sine
        t["asinh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("asinh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("asinh() expects a number", line);
            return XObject::makeFloat(std::asinh(args[0].asNumber()));
        };

        // acosh(x) — inverse hyperbolic cosine
        t["acosh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acosh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acosh() expects a number", line);
            double val = args[0].asNumber();
            if (val < 1.0)
                throw TypeError("acosh() domain error: argument must be >= 1", line);
            return XObject::makeFloat(std::acosh(val));
        };

        // atanh(x) — inverse hyperbolic tangent
        t["atanh"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("atanh", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("atanh() expects a number", line);
            double val = args[0].asNumber();
            if (val <= -1.0 || val >= 1.0)
                throw TypeError("atanh() domain error: argument must be in (-1, 1)", line);
            return XObject::makeFloat(std::atanh(val));
        };

        // acoth(x) — inverse hyperbolic cotangent = atanh(1/x)
        t["acoth"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acoth", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acoth() expects a number", line);
            double val = args[0].asNumber();
            if (val >= -1.0 && val <= 1.0)
                throw TypeError("acoth() domain error: |x| must be > 1", line);
            return XObject::makeFloat(std::atanh(1.0 / val));
        };

        // asech(x) — inverse hyperbolic secant = acosh(1/x)
        t["asech"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("asech", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("asech() expects a number", line);
            double val = args[0].asNumber();
            if (val <= 0.0 || val > 1.0)
                throw TypeError("asech() domain error: argument must be in (0, 1]", line);
            return XObject::makeFloat(std::acosh(1.0 / val));
        };

        // acsch(x) — inverse hyperbolic cosecant = asinh(1/x)
        t["acsch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("acsch", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("acsch() expects a number", line);
            double val = args[0].asNumber();
            if (val == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(std::asinh(1.0 / val));
        };

        // clamp(x, lo, hi) — constrain between lo and hi
        t["clamp"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("clamp", 3, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
                throw TypeError("clamp() expects three numbers", line);
            double x = args[0].asNumber();
            double lo = args[1].asNumber();
            double hi = args[2].asNumber();
            if (x < lo)
                return args[1];
            if (x > hi)
                return args[2];
            return args[0];
        };

        // random() → random float 0.0–1.0
        t["random"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 0)
                throw ArityError("random", 0, (int)args.size(), line);
            static thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return XObject::makeFloat(dist(rng));
        };

        // random_int(a, b) → random integer between a and b (inclusive)
        t["random_int"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("random_int", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("random_int() expects two numbers", line);
            static thread_local std::mt19937 rng(std::random_device{}());
            int64_t a = (int64_t)args[0].asNumber();
            int64_t b = (int64_t)args[1].asNumber();
            std::uniform_int_distribution<int64_t> dist(std::min(a, b), std::max(a, b));
            return XObject::makeInt(dist(rng));
        };

        // random_choice(list) → pick random item from list
        t["random_choice"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("random_choice", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("random_choice() expects a list", line);
            const auto &list = args[0].asList();
            if (list.empty())
                throw IndexError("random_choice() on empty list", line);
            static thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, list.size() - 1);
            return list[dist(rng)];
        };

        // is_nan(x) → bool
        t["is_nan"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_nan", 1, (int)args.size(), line);
            if (args[0].isFloat())
                return XObject::makeBool(std::isnan(args[0].asFloat()));
            if (args[0].isInt())
                return XObject::makeBool(false);
            throw TypeError("is_nan() expects a number", line);
        };

        // is_inf(x) → bool
        t["is_inf"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_inf", 1, (int)args.size(), line);
            if (args[0].isFloat())
                return XObject::makeBool(std::isinf(args[0].asFloat()));
            if (args[0].isInt())
                return XObject::makeBool(false);
            throw TypeError("is_inf() expects a number", line);
        };

        // to_int(x) — truncate to integer (alias for int())
        t["to_int"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_int", 1, (int)args.size(), line);
            if (args[0].isInt())
                return args[0];
            if (args[0].isFloat())
                return XObject::makeInt(static_cast<int64_t>(args[0].asFloat()));
            if (args[0].isString())
            {
                try
                {
                    return XObject::makeInt(std::stoll(args[0].asString()));
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to int", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeInt(args[0].asBool() ? 1 : 0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to int", line);
        };

        // to_float(x) — parse as float
        t["to_float"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_float", 1, (int)args.size(), line);
            if (args[0].isFloat())
                return args[0];
            if (args[0].isInt())
                return XObject::makeFloat(static_cast<double>(args[0].asInt()));
            if (args[0].isString())
            {
                try
                {
                    return XObject::makeFloat(std::stod(args[0].asString()));
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to float", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeFloat(args[0].asBool() ? 1.0 : 0.0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to float", line);
        };

        // hex(n) → string — integer to hex string (no prefix)
        t["hex"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("hex", 1, (int)args.size(), line);
            if (!args[0].isInt() && !args[0].isFloat())
                throw TypeError("hex() expects an integer", line);
            int64_t val = (int64_t)args[0].asNumber();
            std::ostringstream oss;
            if (val < 0)
                oss << "-" << std::hex << (-val);
            else
                oss << std::hex << val;
            return XObject::makeString(oss.str());
        };

        // bin(n) → string — integer to binary string (no prefix)
        t["bin"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("bin", 1, (int)args.size(), line);
            if (!args[0].isInt() && !args[0].isFloat())
                throw TypeError("bin() expects an integer", line);
            int64_t val = (int64_t)args[0].asNumber();
            if (val == 0)
                return XObject::makeString("0");
            bool negative = val < 0;
            uint64_t uval = negative ? (uint64_t)(-val) : (uint64_t)val;
            std::string result;
            while (uval > 0)
            {
                result = (char)('0' + (uval & 1)) + result;
                uval >>= 1;
            }
            if (negative)
                result = "-" + result;
            return XObject::makeString(std::move(result));
        };
    }

} // namespace xell
