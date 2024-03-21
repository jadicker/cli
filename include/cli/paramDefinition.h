#pragma once

#include "param.h"

// TODO: Maybe leaving POD in here is ok?
#include "detail/fromstring.h"

// TODO: Push into specific parameter file
#include <MechSim/Misc/ObjectId.h>

#include <any>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <string>
#include <vector>
#include <fstream>

// Declares a type name
#define NAME_BASIC_TYPE(typeName) template <> struct TypeDesc< typeName > { static const char* Name() { return #typeName; } };

#define NAME_TYPE(typeName, type) template <> struct TypeDesc< type > { static const char* Name() { return #typeName; } };

namespace cli
{
    template <typename T> struct TypeDesc { static const char* Name() { return ""; } };
    template <> struct TypeDesc< char > { static const char* Name() { return "<char>"; } };
    template <> struct TypeDesc< unsigned char > { static const char* Name() { return "<unsigned char>"; } };
    template <> struct TypeDesc< signed char > { static const char* Name() { return "<signed char>"; } };
    template <> struct TypeDesc< short > { static const char* Name() { return "<short>"; } };
    template <> struct TypeDesc< unsigned short > { static const char* Name() { return "<unsigned short>"; } };
    template <> struct TypeDesc< int > { static const char* Name() { return "<int>"; } };
    template <> struct TypeDesc< unsigned int > { static const char* Name() { return "<unsigned int>"; } };
    template <> struct TypeDesc< long > { static const char* Name() { return "<long>"; } };
    template <> struct TypeDesc< unsigned long > { static const char* Name() { return "<unsigned long>"; } };
    template <> struct TypeDesc< long long > { static const char* Name() { return "<long long>"; } };
    template <> struct TypeDesc< unsigned long long > { static const char* Name() { return "<unsigned long long>"; } };
    template <> struct TypeDesc< float > { static const char* Name() { return "<float>"; } };
    template <> struct TypeDesc< double > { static const char* Name() { return "<double>"; } };
    template <> struct TypeDesc< long double > { static const char* Name() { return "<long double>"; } };
    template <> struct TypeDesc< bool > { static const char* Name() { return "<bool>"; } };
    template <> struct TypeDesc< std::string > { static const char* Name() { return "<string>"; } };
    template <> struct TypeDesc< std::vector<std::string> > { static const char* Name() { return "<list of strings>"; } };

    // Strategy: move the template expansion into getting param values back out, avoid expanding
    // every invocation of a command function into new functions, which appears to be crushing
    // debugging.  It also makes reasoning about individual parameter types and the commands themselves
    // a bit tricky, hopefully the approach of using concrete children of Param will make that clearer.
    template<int N, typename... Ts>
    using NthTypeOf = typename std::tuple_element<N, std::tuple<Ts...>>::type;

    template <typename... Result, std::size_t... Indices>
    auto AnyVectorToTupleImpl(const std::vector<std::any>& v, std::index_sequence<Indices...>) {
        return std::make_tuple(
            std::any_cast<NthTypeOf<Indices, Result...>>(v[Indices])...
        );
    }

    template <typename ...Result>
    std::tuple<Result...> AnyVectorToTuple(std::vector<std::any>& values)
    {
        return AnyVectorToTupleImpl<Result...>(values, std::make_index_sequence<sizeof...(Result)>());
    }

#define DECLARE_PARAM(className, type) \
explicit className (std::string name) : Param(std::move(name)) {} \
explicit className (std::string name, type val) : Param(std::move(name)), m_value(std::move(val)) {}

#define DEFINE_PREPARE(resultMember, type) \
    bool Prepare(ParamContext& ctx, const std::string& token) override \
    { \
        auto result = ParseWrapper(ctx, token); \
        if (!result) \
        { \
            return false; \
        } \
        resultMember = std::any_cast<type>(*result); \
        return true; \
    }

    template <typename ...Result>
    std::tuple<Result...> Unpack(std::vector<std::shared_ptr<Param>> params)
    {
        std::vector<std::any> anys;
        std::transform(params.begin(), params.end(), std::back_inserter(anys),
                       [](const std::shared_ptr<Param>& param)
                       {
                           return param->GetValue();
                       });

        return AnyVectorToTuple<Result...>(anys);
    }

    template <typename T>
    class PODParam : public Param
    {
    public:
        DECLARE_PARAM(PODParam, T)

        DEFINE_PREPARE(m_value, T)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            return {};
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return TypeDesc<T>::Name();
        }

    protected:
        T m_value;

        std::optional<T> ParseImpl(const ParamContext& ctx, const std::string& token) const
        {
            try
            {
                return cli::detail::FromString<T>::get(ctx.m_out, GetName(), token);
            }
            catch (cli::detail::bad_conversion&)
            {
                return {};
            }
        }

        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            if (auto result = ParseImpl(ctx, token))
            {
                return *result;
            }

            return {};
        }
    };

    using FloatParam = PODParam<float>;
    using IntParam = PODParam<int>;

    class IntRangeParam : public PODParam<int>
    {
    public:
        IntRangeParam(std::string name, int min, int max)
            : PODParam<int>(std::move(name))
            , m_min(min)
            , m_max(max)
        {
            assert(m_max > m_min);
        }

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            Completions completions;

            // TODO: What to do here?
            const size_t kMaxIterations = 20;
            for (int i = m_min; i <= m_max && i < m_min + kMaxIterations; ++i)
            {
                completions.push_back( { std::to_string(i), "" });
            }
            return completions;
        }

        const char* GetTypeName() const override
        {
            return "Range";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            try
            {
                int val = cli::detail::FromString<int>::get(ctx.m_out, GetName(), token);
                if (val < m_min || val > m_max)
                {
                    return {};
                }
                int clampedVal = Util::Clamp(val, m_min, m_max);
                return clampedVal;
            }
            catch (cli::detail::bad_conversion&)
            {
                return {};
            }
        }

    private:
        int m_min;
        int m_max;
    };

    class FloatRangeParam : public PODParam<float>
    {
    public:
        FloatRangeParam(std::string name, float min, float max)
                : PODParam<float>(std::move(name))
                , m_min(min)
                , m_max(max)
        {
            assert(m_max > m_min);
        }

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            Completions completions;

            // TODO: What to do here?
            return completions;
        }

        const char* GetTypeName() const override
        {
            return "FloatRange";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            try
            {
                float val = cli::detail::FromString<float>::get(ctx.m_out, GetName(), token);
                if (val < m_min || val > m_max)
                {
                    return {};
                }
                float clampedVal = Util::Clamp(val, m_min, m_max);
                return clampedVal;
            }
            catch (cli::detail::bad_conversion&)
            {
                return {};
            }
        }

    private:
        float m_min;
        float m_max;
    };

    class StringParam : public Param
    {
    public:
        DECLARE_PARAM(StringParam, std::string)

        DEFINE_PREPARE(m_value, std::string)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            return {};
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return "string";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            return token;
        }

    private:
        std::string m_value;
    };

    class ObjectIdParam : public Param
    {
    public:
        DECLARE_PARAM(ObjectIdParam, MechSim::ObjectId)

        DEFINE_PREPARE(m_value, MechSim::ObjectId)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            return {};
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return "ObjectId";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            // TODO: implement
            return MechSim::MakeObjectId(1, 2, 3, 4);
        }

    private:
        MechSim::ObjectId m_value;
    };
}