#pragma once

#include "pch.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    class JsonWriter final
    {
    public:
        JsonWriter();

        void BeginObject();
        void EndObject();
        void BeginArrayProperty(std::wstring_view name);
        void EndArray();
        void BeginObjectProperty(std::wstring_view name);
        void StringProperty(std::wstring_view name, std::wstring_view value);
        void IntProperty(std::wstring_view name, long long value);
        void UIntProperty(std::wstring_view name, unsigned long long value);
        void BoolProperty(std::wstring_view name, bool value);
        void NullProperty(std::wstring_view name);
        void RawProperty(std::wstring_view name, std::wstring_view rawJson);
        void BeginArrayItemObject();
        void EndArrayItemObject();

        [[nodiscard]] std::wstring ToString() const;
        [[nodiscard]] static std::wstring Escape(std::wstring_view value);

    private:
        enum class ScopeKind
        {
            Object,
            Array,
        };

        struct Scope
        {
            ScopeKind Kind;
            bool First = true;
        };

        void BeforeValue();
        void WritePropertyName(std::wstring_view name);

        std::wostringstream m_stream;
        std::vector<Scope> m_scopes;
    };
}
