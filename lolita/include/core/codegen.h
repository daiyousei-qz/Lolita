#pragma once
#include "text/format.h"
#include <functional>
#include <string_view>
#include <sstream>

namespace eds::loli::codegen
{
    class CppEmitter
    {
    public:
        using CallbackType = std::function<void()>;

        // line
        //
        void EmptyLine()
        {
            buffer_ << std::endl;
        }
        void Comment(std::string_view s)
        {
            WriteIdent();
            buffer_ << "// " << s << std::endl;
        }
        void Include(std::string_view s, bool system)
        {
            WriteIdent();
            buffer_ << "#include ";
            buffer_ << (system ? '<' : '"');
            buffer_ << s;
            buffer_ << (system ? '>' : '"');
            buffer_ << std::endl;
        }

        template <typename... TArgs>
        void WriteLine(const char* fmt, const TArgs&... args)
        {
            WriteIdent();
            buffer_ << text::Format(fmt, args...) << std::endl;
        }

        // structure
        //

        void Namespace(std::string_view name, CallbackType cbk)
        {
            WriteStructure("namespace", name, "", false, cbk);
        }
        void Class(std::string_view name, std::string_view parent, CallbackType cbk)
        {
            WriteStructure("class", name, parent, true, cbk);
        }
        void Struct(std::string_view name, std::string_view parent, CallbackType cbk)
        {
            WriteStructure("struct", name, parent, true, cbk);
        }
        void Enum(std::string_view name, std::string_view type, CallbackType cbk)
        {
            WriteStructure("enum", name, type, true, cbk);
        }

        // block
        //

        void Block(std::string_view header, CallbackType cbk)
        {
            WriteBlock(header, false, cbk);
        }

        std::string ToString()
        {
            return buffer_.str();
        }

    private:
        void WriteIdent()
        {
            for (int i = 0; i < ident_level_; ++i)
                buffer_ << "    ";
        }

        void WriteBlock(std::string_view header, bool semi, CallbackType cbk)
        {
            WriteIdent();
            buffer_ << header << std::endl;

            WriteIdent();
            buffer_ << "{\n";

            // body
            ident_level_ += 1;
            cbk();
            ident_level_ -= 1;

            WriteIdent();
            buffer_ << (semi ? "};\n" : "}\n");
        }

        void WriteStructure(std::string_view type, std::string_view name, std::string_view parent, bool semi, CallbackType cbk)
        {
            auto header = text::Format("{} {}", type, name);
            if (!parent.empty())
            {
                header.append(" : ").append(parent);
            }

            WriteBlock(header, semi, cbk);
        }

        int ident_level_ = 0;
        std::ostringstream buffer_;
    };
}