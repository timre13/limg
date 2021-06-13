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

#include "XmlParser.h"
#include "Logger.h"
#include <string>
#include <iostream>

static void printElementInfo(const XmlElement& element)
{
    switch (element.getType())
    {
        case XmlElement::Type::OpeningElement:
            std::cout << "\033[32m";
            break;
        case XmlElement::Type::ClosingElement:
            std::cout << "\033[31m";
            break;
        case XmlElement::Type::Content:
            std::cout << "\033[34m";
            break;
        case XmlElement::Type::SelfclosingElement:
            std::cout << "\033[33m";
            break;
    }
    std::cout
        << "Name: " << element.getElementName()
        << "\nType: " << element.getTypeStr()
        << "\nAttributes:\n";
    for (auto& attribute : element.attributes())
        std::cout << '\t' << attribute.first << " = " << attribute.second << '\n';
    std::cout << "\033[0m\n";
}

XmlParser::XmlParser(const std::string& document)
{
    Logger::log << "Splitting XML document" << Logger::End;

    std::vector<std::string> tokens;
    std::string currentToken;
    for (size_t i{}; i < document.size(); ++i)
    {
        char currentChar = document[i];

        switch (currentChar)
        {
        case '<':
            tokens.push_back(currentToken);
            currentToken.clear();
            currentToken += currentChar;
            break;

        case '>':
            currentToken += currentChar;
            tokens.push_back(currentToken);
            currentToken.clear();
            break;

        default:
            currentToken += currentChar;
            break;
        }
    }

    Logger::log << "Parsing elements" << Logger::End;

    for (auto& token : tokens)
    {
        XmlElement::Type elementType{};
        std::string fullElement;

        {
            size_t i{};
            while (i < token.size() && std::isspace(token[i]))
                ++i;
    
            while (i < token.size())
                fullElement += token[i++];
        }

        if (fullElement[0] == '<' && fullElement[1] == '/')
        {
            elementType = XmlElement::Type::ClosingElement;
            fullElement = fullElement.substr(2, fullElement.size()-1-2);
        }
        else if (fullElement[fullElement.size()-2] == '/')
        {
            elementType = XmlElement::Type::SelfclosingElement;
            fullElement = fullElement.substr(1, fullElement.size()-1-1-1);
        }
        else if (fullElement[0] == '<')
        {
            elementType = XmlElement::Type::OpeningElement;
            fullElement = fullElement.substr(1, fullElement.size()-1-1);
        }
        else
        {
            elementType = XmlElement::Type::Content;
        }

        if (fullElement.empty())
            continue;

        std::string elementName;
        if (elementType != XmlElement::Type::Content)
        {
            size_t i{};
            while (i < fullElement.size() && (std::isalpha(fullElement[i]) || fullElement[i] == ':' || fullElement[i] == '?'))
            elementName += fullElement[i++];
        }
        else
        {
            elementName = fullElement;
        }

        XmlElement element{elementName, elementType};

        {
            size_t i{};

            while (i < fullElement.size())
            {
                std::string key;
                std::string value;

                // Skip element name
                while (i < fullElement.size() && !std::isspace(fullElement[i]))
                    ++i;

                // Go to key name
                while (i < fullElement.size() && !(std::isalpha(fullElement[i]) || fullElement[i] == ':'))
                    ++i;

                // Get key
                while (i < fullElement.size() && (std::isalpha(fullElement[i]) || fullElement[i] == ':'))
                    key += fullElement[i++];

                // Go to value
                while (i < fullElement.size() && fullElement[i] != '"')
                    ++i;
                ++i;

                // Get value
                while (i < fullElement.size() && fullElement[i] != '"')
                    value += fullElement[i++];

                if (key.size())
                    element.addAttribute(key, value);
            }
        }
        
        printElementInfo(element);

        m_elements.push_back(std::move(element));
    }
}

