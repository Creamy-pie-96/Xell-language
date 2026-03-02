#pragma once

// =============================================================================
// DateTime builtins — now, timestamp, timestamp_ms, format_date, parse_date,
//                     sleep, sleep_sec, time_since
// =============================================================================

#include "builtin_registry.hpp"
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace xell
{

    inline void registerDateTimeBuiltins(BuiltinTable &t)
    {
        // now() — returns current date/time as a map
        // {year, month, day, hour, minute, second, weekday, yearday}
        t["now"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 0)
                throw ArityError("now", 0, (int)args.size(), line);

            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm *tm = std::localtime(&t);

            XMap result;
            result.set("year", XObject::makeInt(1900 + tm->tm_year));
            result.set("month", XObject::makeInt(1 + tm->tm_mon));
            result.set("day", XObject::makeInt(tm->tm_mday));
            result.set("hour", XObject::makeInt(tm->tm_hour));
            result.set("minute", XObject::makeInt(tm->tm_min));
            result.set("second", XObject::makeInt(tm->tm_sec));
            result.set("weekday", XObject::makeInt(tm->tm_wday));     // 0=Sunday
            result.set("yearday", XObject::makeInt(1 + tm->tm_yday)); // 1-based

            return XObject::makeMap(std::move(result));
        };

        // timestamp() — Unix timestamp in seconds (integer)
        t["timestamp"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 0)
                throw ArityError("timestamp", 0, (int)args.size(), line);

            auto now = std::chrono::system_clock::now();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch())
                            .count();
            return XObject::makeInt(static_cast<int64_t>(secs));
        };

        // timestamp_ms() — Unix timestamp in milliseconds (integer)
        t["timestamp_ms"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 0)
                throw ArityError("timestamp_ms", 0, (int)args.size(), line);

            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count();
            return XObject::makeInt(static_cast<int64_t>(ms));
        };

        // format_date(date_map, fmt) — format a date map using strftime patterns
        t["format_date"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("format_date", 2, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("format_date() expects a date map as first argument", line);
            if (!args[1].isString())
                throw TypeError("format_date() expects a format string as second argument", line);

            const auto &m = args[0].asMap();
            std::tm tm = {};

            auto getField = [&](const std::string &key, int def) -> int
            {
                auto *val = m.get(key);
                if (val && val->isNumber())
                    return (int)val->asNumber();
                return def;
            };

            tm.tm_year = getField("year", 1900) - 1900;
            tm.tm_mon = getField("month", 1) - 1;
            tm.tm_mday = getField("day", 1);
            tm.tm_hour = getField("hour", 0);
            tm.tm_min = getField("minute", 0);
            tm.tm_sec = getField("second", 0);
            tm.tm_wday = getField("weekday", 0);
            tm.tm_yday = getField("yearday", 1) - 1;

            char buf[256];
            size_t len = std::strftime(buf, sizeof(buf), args[1].asString().c_str(), &tm);
            if (len == 0)
                throw RuntimeError("format_date() failed to format date", line);

            return XObject::makeString(std::string(buf, len));
        };

        // parse_date(str, fmt) — parse a date string into a map
        t["parse_date"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("parse_date", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("parse_date() expects two strings", line);

            std::tm tm = {};
            std::istringstream ss(args[0].asString());
            ss >> std::get_time(&tm, args[1].asString().c_str());
            if (ss.fail())
                throw RuntimeError("parse_date() failed to parse '" + args[0].asString() + "' with format '" + args[1].asString() + "'", line);

            XMap result;
            result.set("year", XObject::makeInt(1900 + tm.tm_year));
            result.set("month", XObject::makeInt(1 + tm.tm_mon));
            result.set("day", XObject::makeInt(tm.tm_mday));
            result.set("hour", XObject::makeInt(tm.tm_hour));
            result.set("minute", XObject::makeInt(tm.tm_min));
            result.set("second", XObject::makeInt(tm.tm_sec));

            return XObject::makeMap(std::move(result));
        };

        // sleep(ms) — pause for N milliseconds
        t["sleep"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sleep", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sleep() expects a number (milliseconds)", line);

            int ms = (int)args[0].asNumber();
            if (ms > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));

            return XObject::makeNone();
        };

        // sleep_sec(s) — pause for N seconds
        t["sleep_sec"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sleep_sec", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sleep_sec() expects a number (seconds)", line);

            double secs = args[0].asNumber();
            if (secs > 0)
                std::this_thread::sleep_for(
                    std::chrono::milliseconds((int)(secs * 1000)));

            return XObject::makeNone();
        };

        // time_since(ts) — seconds elapsed since a timestamp (float)
        t["time_since"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("time_since", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("time_since() expects a numeric timestamp", line);

            double pastTs = args[0].asNumber();
            auto now = std::chrono::system_clock::now();
            double nowTs = (double)std::chrono::duration_cast<std::chrono::milliseconds>(
                               now.time_since_epoch())
                               .count() /
                           1000.0;

            return XObject::makeFloat(nowTs - pastTs);
        };
    }

} // namespace xell
