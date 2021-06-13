/*
BSD 2-Clause License

Copyright (c) 2021, timre13
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <iterator>
#include <vector>
#include <string>
#include <cctype>
#include <map>

class XmlElement final
{
public:
    enum class Type
    {
        OpeningElement,
        ClosingElement,
        SelfclosingElement,
        Content,
    };

private:
    std::string m_elementName;
    Type m_type;
    std::map<std::string, std::string> m_attributes;

public:
    XmlElement(const std::string& elementName, Type type)
        : m_elementName{elementName}, m_type{type}
    {
    }

    const std::string& getElementName() const { return m_elementName; }

    static std::string typeToStr(Type type)
    {
        switch (type)
        {
        case Type::OpeningElement:
            return "Opening element";
        case Type::ClosingElement:
            return "Closing element";
        case Type::SelfclosingElement:
            return "Self-closing element";
        case Type::Content:
            return "Content";
        }
    }
    Type getType() const { return m_type; }
    std::string getTypeStr() const { return typeToStr(m_type); }

    // Attribute (s/g)etters
    std::string getAttribute(const std::string& key) const { try { return m_attributes.at(key); } catch (...) { return ""; } }
    void addAttribute(const std::string& key, const std::string& value) { m_attributes.insert({key, value}); }
    const std::map<std::string, std::string>& attributes() const { return m_attributes; }
};

class XmlParser final
{
private:
    using elementList_t = std::vector<XmlElement>;
    elementList_t m_elements;

public:
    XmlParser(const std::string& document);

    // Iterators
    inline elementList_t::const_iterator begin() const { return m_elements.begin(); }
    inline elementList_t::iterator       begin()       { return m_elements.begin(); }
    inline elementList_t::const_iterator end()   const { return m_elements.end();   }
    inline elementList_t::iterator       end()         { return m_elements.end();   }

    inline size_t size() const { return m_elements.size(); }

    const XmlElement* findFirstElementWithName(const std::string& name)
    {
        for (auto& element : m_elements)
        {
            if (element.getElementName().compare(name) == 0)
                return &element;
        }

        return nullptr;
    }
};

