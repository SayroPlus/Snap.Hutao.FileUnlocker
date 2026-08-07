#include "pch.h"
#include "JsonWriter.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    JsonWriter::JsonWriter()
    {
    }

    void JsonWriter::BeginObject()
    {
        BeforeValue();
        m_stream << L'{';
        m_scopes.push_back({ ScopeKind::Object });
    }

    void JsonWriter::EndObject()
    {
        m_stream << L'}';
        m_scopes.pop_back();
    }

    void JsonWriter::BeginArrayProperty(std::wstring_view name)
    {
        WritePropertyName(name);
        m_stream << L'[';
        m_scopes.push_back({ ScopeKind::Array });
    }

    void JsonWriter::EndArray()
    {
        m_stream << L']';
        m_scopes.pop_back();
    }

    void JsonWriter::BeginObjectProperty(std::wstring_view name)
    {
        WritePropertyName(name);
        m_stream << L'{';
        m_scopes.push_back({ ScopeKind::Object });
    }

    void JsonWriter::StringProperty(std::wstring_view name, std::wstring_view value)
    {
        WritePropertyName(name);
        m_stream << L'"' << Escape(value) << L'"';
    }

    void JsonWriter::IntProperty(std::wstring_view name, long long value)
    {
        WritePropertyName(name);
        m_stream << value;
    }

    void JsonWriter::UIntProperty(std::wstring_view name, unsigned long long value)
    {
        WritePropertyName(name);
        m_stream << value;
    }

    void JsonWriter::BoolProperty(std::wstring_view name, bool value)
    {
        WritePropertyName(name);
        m_stream << (value ? L"true" : L"false");
    }

    void JsonWriter::NullProperty(std::wstring_view name)
    {
        WritePropertyName(name);
        m_stream << L"null";
    }

    void JsonWriter::RawProperty(std::wstring_view name, std::wstring_view rawJson)
    {
        WritePropertyName(name);
        m_stream << rawJson;
    }

    void JsonWriter::BeginArrayItemObject()
    {
        BeforeValue();
        m_stream << L'{';
        m_scopes.push_back({ ScopeKind::Object });
    }

    void JsonWriter::EndArrayItemObject()
    {
        EndObject();
    }

    std::wstring JsonWriter::ToString() const
    {
        return m_stream.str();
    }

    std::wstring JsonWriter::Escape(std::wstring_view value)
    {
        std::wstring result;
        result.reserve(value.size());

        for (wchar_t ch : value)
        {
            switch (ch)
            {
            case L'\\':
                result.append(L"\\\\");
                break;
            case L'"':
                result.append(L"\\\"");
                break;
            case L'\b':
                result.append(L"\\b");
                break;
            case L'\f':
                result.append(L"\\f");
                break;
            case L'\n':
                result.append(L"\\n");
                break;
            case L'\r':
                result.append(L"\\r");
                break;
            case L'\t':
                result.append(L"\\t");
                break;
            default:
                if (ch < 0x20)
                {
                    wchar_t buffer[7]{};
                    swprintf_s(buffer, L"\\u%04x", static_cast<unsigned int>(ch));
                    result.append(buffer);
                }
                else
                {
                    result.push_back(ch);
                }
                break;
            }
        }

        return result;
    }

    void JsonWriter::BeforeValue()
    {
        if (m_scopes.empty())
        {
            return;
        }

        Scope& scope = m_scopes.back();
        if (!scope.First)
        {
            m_stream << L',';
        }

        scope.First = false;
    }

    void JsonWriter::WritePropertyName(std::wstring_view name)
    {
        BeforeValue();
        m_stream << L'"' << Escape(name) << L"\":";
    }
}
