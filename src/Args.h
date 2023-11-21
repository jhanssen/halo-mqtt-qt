#pragma once

#include <QString>
#include <QList>
#include <QHash>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <typeinfo>
#include <any>

namespace args {

class Parser;

class Args
{
public:
    ~Args();

    bool has(const QString& key) const;
    template<typename T>
    bool has(const QString& key) const;

    template<typename T, typename std::enable_if<!std::is_trivial_v<T>>::type* = nullptr>
    T value(const QString& key, const T& defaultValue = T()) const;

    template<typename T, typename std::enable_if<std::is_trivial_v<T> && !std::is_floating_point_v<T>>::type* = nullptr>
    T value(const QString& key, T defaultValue = T()) const;

    template<typename T, typename std::enable_if<std::is_trivial_v<T> && std::is_floating_point_v<T>>::type* = nullptr>
    T value(const QString& key, T defaultValue = T()) const;

    size_t freeformSize() const;
    QString freeformValue(size_t idx) const;

private:
    Args();

    friend class Parser;

private:
    QHash<QString, std::any> mValues;
    QList<QString> mFreeform;
};

inline Args::Args()
{
}

inline Args::~Args()
{
}

inline bool Args::has(const QString& key) const
{
    return mValues.count(key) > 0;
}

template<typename T>
inline bool Args::has(const QString& key) const
{
    auto v = mValues.find(key);
    if (v == mValues.end())
        return false;
    if (v.value().type() == typeid(T))
        return true;
    if (v.value().type() == typeid(int64_t)) {
        if (typeid(T) == typeid(int32_t) ||
            typeid(T) == typeid(float) ||
            typeid(T) == typeid(double))
            return true;
        return false;
    }
    if (typeid(T) == typeid(float) && v.value().type() == typeid(double))
        return true;
    if (typeid(T) == typeid(bool) && v.value().type() == typeid(int64_t))
        return true;
    return false;
}

template<typename T, typename std::enable_if<!std::is_trivial_v<T>>::type*>
inline T Args::value(const QString& key, const T& defaultValue) const
{
    auto v = mValues.find(key);
    if (v == mValues.end())
        return defaultValue;
    if (v.value().type() == typeid(T))
        return std::any_cast<T>(v.value());
    return defaultValue;
}

template<typename T, typename std::enable_if<std::is_trivial_v<T> && !std::is_floating_point_v<T>>::type*>
inline T Args::value(const QString& key, T defaultValue) const
{
    auto v = mValues.find(key);
    if (v == mValues.end())
        return defaultValue;
    if (v.value().type() == typeid(T))
        return std::any_cast<T>(v.value());
    if (typeid(T) == typeid(int32_t) && v.value().type() == typeid(int64_t))
        return static_cast<T>(std::any_cast<int64_t>(v.value()));
    if (typeid(T) == typeid(bool) && v.value().type() == typeid(int64_t))
        return static_cast<T>(std::any_cast<int64_t>(v.value()));
    return defaultValue;
}

template<typename T, typename std::enable_if<std::is_trivial_v<T> && std::is_floating_point_v<T>>::type*>
inline T Args::value(const QString& key, T defaultValue) const
{
    auto v = mValues.find(key);
    if (v == mValues.end())
        return defaultValue;
    if (v.value().type() == typeid(double))
        return static_cast<T>(std::any_cast<double>(v.value()));
    if (v.value().type() == typeid(int64_t))
        return static_cast<T>(std::any_cast<int64_t>(v.value()));
    return defaultValue;
}

inline size_t Args::freeformSize() const
{
    return mFreeform.size();
}

inline QString Args::freeformValue(size_t idx) const
{
    if (static_cast<qsizetype>(idx) >= mFreeform.size())
        return QString();
    return mFreeform[idx];
}

class Parser
{
public:
    template<typename Error>
    static Args parse(int argc, char** argv, char** envp, const char* envprefix, Error&& error);
    static std::any guessValue(const QString& val);

private:
    Parser() = delete;
};

inline std::any Parser::guessValue(const QString& val)
{
    if (val.isEmpty())
        return std::any(true);
    if (val == "true")
        return std::any(true);
    if (val == "false")
        return std::any(false);
    char* endptr;
    const auto& ba = val.toUtf8();
    long long l = strtoll(ba.constData(), &endptr, 0);
    if (*endptr == '\0')
        return std::any(static_cast<int64_t>(l));
    double d = strtod(ba.constData(), &endptr);
    if (*endptr == '\0')
        return std::any(d);
    return std::any(val);
}

template<typename Error>
inline Args Parser::parse(int argc, char** argv, char** envp, const char* envprefix, Error&& error)
{
    Args args;

    enum State { Normal, Dash, DashDash, Value, Freeform, FreeformOnly };
    State state = Normal;

    QString key;

    auto add = [&key, &args](State s, char* start, char* end) {
        assert(s != Normal);
        switch (s) {
        case Dash:
            while (start < end - 1) {
                args.mValues[QString(*(start++))] = std::any(true);
            }
            break;
        case DashDash:
            key = QString::fromUtf8(start, end - 1 - start);
            break;
        case Value: {
            const auto v = guessValue(QString::fromUtf8(start, end - 1 - start));
            if (v.type() == typeid(bool)) {
                const auto& ba = key.toUtf8();
                if (key.size() > 3 && !strncmp(ba.constData(), "no-", 3)) {
                    args.mValues[key.mid(3)] = std::any(!std::any_cast<bool>(std::move(v)));
                    break;
                }
                if (key.size() > 8 && !strncmp(ba.constData(), "disable-", 8)) {
                    args.mValues[key.mid(8)] = std::any(!std::any_cast<bool>(std::move(v)));
                    break;
                }
            }
            args.mValues[key] = std::move(v);
            break; }
        case Freeform:
            args.mFreeform.push_back(QString::fromUtf8(start, end - 1 - start));
            break;
        default:
            break;
        }
    };

    size_t off = 0;
    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];
        char* argStart = arg;
        char* prev = arg;
        bool done = false;
        while (!done) {
            switch (*(arg++)) {
            case '-':
                switch (state) {
                case Normal:
                    prev = arg;
                    state = Dash;
                    continue;
                case Dash:
                    if (prev == arg - 1) {
                        ++prev;
                        state = DashDash;
                    } else {
                        error("unexpected dash", off + arg - argStart, argStart);
                        return Args();
                    }
                    continue;
                case Freeform:
                case FreeformOnly:
                case DashDash:
                case Value:
                    if (arg - 1 == argStart) {
                        // add value as empty and keep going with dash
                        add(Value, arg, arg + 1);
                        prev = arg;
                        state = Dash;
                    }
                    continue;
                default:
                    error("unexpected dash", off + arg - argStart, argStart);
                    return Args();
                }
                break;
            case '\0':
                done = true;
                switch (state) {
                case Normal:
                    add(Freeform, prev, arg);
                    prev = arg;
                    continue;
                case Dash:
                    add(Dash, prev, arg);
                    prev = arg;
                    state = Normal;
                    continue;
                case DashDash:
                    if (arg - argStart == 3) { // 3 = --\0
                        prev = arg;
                        state = FreeformOnly;
                    } else {
                        add(DashDash, prev, arg);
                        if (i + 1 == argc) {
                            add(Value, arg, arg + 1);
                            prev = arg;
                            state = Normal;
                        } else {
                            prev = arg;
                            state = Value;
                        }
                    }
                    continue;
                case Freeform:
                    state = Normal;
                    // fall through
                case FreeformOnly:
                    add(Freeform, prev, arg);
                    prev = arg;
                    continue;
                case Value:
                    if (arg - 1 == prev) {
                        // missing value
                        error("missing value", off + arg - argStart, argStart);
                        return Args();
                    }
                    add(Value, prev, arg);
                    prev = arg;
                    state = Normal;
                    continue;
                default:
                    error("unexpected state", off + arg - argStart, argStart);
                    return Args();
                }
                break;
            case '=':
                switch (state) {
                case Freeform:
                case FreeformOnly:
                case Value:
                    continue;
                case DashDash:
                    add(DashDash, prev, arg);
                    prev = arg;
                    state = Value;
                    continue;
                default:
                    error("unexpected equals", off + arg - argStart, argStart);
                    return Args();
                }
                break;
            default:
                if (state == Normal && argStart == arg - 1) {
                    prev = arg - 1;
                    state = Freeform;
                }
                break;
            }
        }
        off += arg - argStart;
    }

    auto processEnvKey = [](const QByteArray& key, int start, int end) -> QByteArray {
        // tolower and replace _ with -
        return key.mid(start, end - start).toLower().replace('_', '-');
    };

    const QByteArray qenvprefix = envprefix;
    for (char** env = envp; *env != nullptr; ++env) {
        const QByteArray qenv(*env);
        if (qenv.startsWith(qenvprefix)) {
            // find the equal and extract the key/value
            const auto eq = qenv.indexOf('=');
            if (eq == -1) {
                // nope
                continue;
            }
            const auto key = processEnvKey(qenv, qenvprefix.size(), eq);
            args.mValues[QString::fromUtf8(key)] = guessValue(QString::fromUtf8(qenv.mid(eq + 1)));
        }
    }

    return args;
}

} // namespace args
