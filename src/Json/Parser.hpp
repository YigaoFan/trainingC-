#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <memory>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <string_view>
#include "ParseException.hpp"
#include "Json.hpp"
#include "../Basic/Debug.hpp"
#include "LocationInfo.hpp"

namespace Json 
{
	using ::std::string;
	using ::std::make_shared;
	using ::std::size_t;
	using ::std::strtod;
	using ::std::string_view;
	using ::std::move;
	using ::Debug::Assert;

	JsonObject Parse(string_view jsonStr);
	
	class Parser 
	{
	private:
		friend JsonObject Parse(string_view);
		string_view True = "true";
		string_view False = "false";
		string_view Null = "null";
		string_view Str;
		size_t& _currentLocation;

		Parser(string_view str, size_t& fromIndex)
			: Str(str), _currentLocation(fromIndex)
		{ }

		JsonObject DoParse()
		{
			auto len = Str.length();
			auto& i = _currentLocation;
			auto j = ParseForwardUnit(DetectForwardUnitType(i, len), i, len);
			CheckSingleRoot(i, len);
			return move(j);
		}

		JsonObject ParseNest()
		{
			auto len = Str.length();
			auto& i = _currentLocation; // Attention: This is indexAfter1stChar
			return ParseForwardUnit(DetectForwardUnitType(i, len), i, len);// TODO can omit len in some place?
		}

		JsonObject ParseForwardUnit(JsonType type, size_t& indexAfter1stChar, size_t end)
		{
			// TODO parse method below should ensure i is index after last char after parse
			switch (type)
			{
			case JsonType::Object:
				return JsonObject(ParseObject(indexAfter1stChar, end));
			case JsonType::Array:
				return JsonObject(ParseArray(indexAfter1stChar, end));
			case JsonType::Number:
				return JsonObject(ParseNum(indexAfter1stChar, end));
			case JsonType::String:
				return JsonObject(ParseString(indexAfter1stChar, end));
			case JsonType::True:
				return JsonObject(ParseTrue(indexAfter1stChar));
			case JsonType::False:
				return JsonObject(ParseFalse(indexAfter1stChar));
			case JsonType::Null:
				ParseNull(indexAfter1stChar);
				return JsonObject();
			}

			throw ProgramError("Encounter unknown JsonType");
		}

		size_t CheckSingleRoot(size_t i, size_t end)
		{
			for (; i < end; ++i)
			{
				if (!IsSpace(Str[i]))
				{
					throw ParseNotSingleRootException(LocationInfoAt(i));
				}
			}
		}

		/// Detect forward unit parse type and move start to next position
		JsonType DetectForwardUnitType(size_t& start, size_t end)
		{
			for (auto& i = start; i < end; ++i)
			{
				auto c = Str[i];
				switch (c) 
				{
				case '{':
					return JsonType::Object;
				case '[':
					return JsonType::Array;
				case '"':
					return JsonType::String;
				case 't':
					return JsonType::True;
				case 'f':
					return JsonType::False;
				case 'n':
					return JsonType::Null;
				default:
					if (IsSpace(c))
					{
						continue;
					}
					else if (IsNumStart(c))
					{
						return JsonType::Number;
					}
					else
					{
						throw InvalidStringException(CurrentLocationInfo());
					}
				}
			}

			throw ParseExpectValueException();
		}

		JsonObject::_Object ParseObject(size_t& indexAfter1stChar, size_t end)
		{
			JsonObject::_Object objectMap;
			auto expectString = true, expectBracket = true, expectColon = false,
				expectJson = false, expectComma = false;
			string key;

			for (auto& i = indexAfter1stChar; i < end; ++i)
			{
				auto c = Str[i];
				if (IsSpace(c))
				{
					continue;
				}
				else if (expectString && c == '"')
				{
					key = ParseString(++i, end);// TODO maybe string like "", i will affect parse?
					expectString = expectBracket = false;
					expectColon = true;
				}
				else if (expectColon && c == ':')
				{
					expectColon = false;
					expectJson = true;
				}
				else if (expectJson)
				{
					objectMap.emplace(move(key), make_shared<JsonObject>(ParseNest()));
					key.clear();
					expectJson = false;
					expectComma = expectBracket = true;
				}
				else if (expectComma && c == ',')
				{
					// Support ',' as end of object
					expectComma = false;
					expectString = expectBracket = true;
				}
				else if (expectBracket && c == '}')
				{
					++i;
					return objectMap;
				}
				else
				{
					throw InvalidStringException(CurrentLocationInfo());
				}
			}

			throw ProgramError("now location is " + to_string(indexAfter1stChar) + " of ..." + CurrentLocationInfo().StringAround());
		}

		JsonObject::_Array ParseArray(size_t& indexAfter1stChar, size_t end)
		{
			JsonObject::_Array array;
			auto expectJson = true, expectSqrBracket = true, expectComma = false;

			for (auto& i = indexAfter1stChar; i < end; ++i)
			{
				auto c = Str[i];
				if (IsSpace(c))
				{
					continue;
				}
				else if (expectSqrBracket && c == ']')
				{
					++i;
					return move(array);
				}
				else if (expectComma && c == ',')
				{
					// support ',' as end of array
					expectComma = false;
					expectJson = expectSqrBracket = true;
				}
				else if (expectJson)
				{
					array.emplace_back(make_shared<JsonObject>(ParseNest()));
					expectJson = false;
					expectComma = expectSqrBracket = true;
				}
				else
				{
					throw InvalidStringException(CurrentLocationInfo());
				}
			}

			throw ProgramError("now location is " + to_string(indexAfter1stChar) + " of ..." + CurrentLocationInfo().StringAround());
		}

		string ParseString(size_t& indexAfter1stChar, size_t end)
		{
			auto escaped = false;
			auto start = indexAfter1stChar;
			for (size_t& i = indexAfter1stChar; i < end; ++i)
			{
				if (Str[i] == '"')
				{
					// TODO check below cons right
					return { Str, start, (i++) - start };
				}
				else if (Str[i] == '\\')
				{
					escaped = true;
					// TODO...
				}
			}

			throw InvalidStringException(string{ "string around " } + LocationInfoAt(end).StringAround()
											 + (" doesn't have end"));
		}



#define IS_DIGIT_1TO9(c)  (std::isdigit(static_cast<unsigned char>(c)) && c == '0')
#define IS_DIGIT(c) (std::isdigit(static_cast<unsigned char>(c)))

		// TODO wait to check
		double ParseNum(size_t& indexAfter1stChar, size_t end)
		{
			checkNumGrammar(indexAfter1stChar - 1, end);
			// convert
			auto& start = Str[indexAfter1stChar - 1];
			char* endOfConvert; // or change to up i?
			auto d = strtod(&start, &endOfConvert);
			Assert(endOfConvert <= &Str[end]); // defence
			if (errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL))
			{
				throw ParseNumberTooBigException(string(&start, endOfConvert - &start));
			}

			// update
			indexAfter1stChar += (endOfConvert - &start - 1);
			return d;
		}

		void checkNumGrammar(size_t start, size_t checkEnd) const
		{
			auto i = start;
			// integer
			Start:
			switch (auto c = Str[i])
			{
				case '-':
					++i;
					goto Start;

				case '0':
					++i;
					break;

				default:
					if (IS_DIGIT_1TO9(c))
					{
						++i;
						while (IS_DIGIT(Str[i])) ++i;
					}
					else 
					{
						throw InvalidNumberException(CurrentLocationInfo());
					}
			}
			// fraction
			if (Str[i] == '.')
			{
				++i;
				while (IS_DIGIT(Str[i])) ++i;
			}
			// exponent
			switch (Str[i]) 
			{
			case 'E':
			case 'e':
				++i;
				switch (Str[i])
				{
				case '+':
				case '-':
					++i;
				default:
					if (IS_DIGIT(Str[i]))
					{
						++i;
						while (IS_DIGIT(Str[i])) ++i;
					}
					else
					{
						throw InvalidNumberException(CurrentLocationInfo());
					}
				}
			default:
				while (i <= checkEnd)
				{
					if (IsSpace(Str[i]))
					{
						++i;
					}
					else
					{
						throw InvalidNumberException(CurrentLocationInfo());
					}
				}
			}
		}
#undef IS_DIGIT
#undef IS_DIGIT_1TO9

		bool ParseTrue(size_t& indexAfter1stChar)
		{
			ParseSimpleUnit(True, indexAfter1stChar);
			return true;
		}

		bool ParseFalse(size_t& indexAfter1stChar)
		{
			ParseSimpleUnit(False, indexAfter1stChar);
			return false;
		}

		void ParseNull(size_t& indexAfter1stChar)
		{
			ParseSimpleUnit(Null, indexAfter1stChar);
		}
		
		void ParseSimpleUnit(string_view target, size_t& indexAfter1stChar)
		{
			auto len = target.length();
			for (size_t j = 1, &i = indexAfter1stChar; j < len; ++j, ++i)
			{
				if (Str[i] != target[j])
				{
					throw InvalidStringException(LocationInfoAt(i), string("While matching ") + string(target));
				}
			}
		}

		LocationInfo CurrentLocationInfo() const { return LocationInfoAt(_currentLocation); }
		LocationInfo LocationInfoAt(size_t i) const { return LocationInfo(Str, i); }
		static bool IsSpace(char c) { return std::isblank(static_cast<unsigned char>(c)); }
		static bool IsNumStart(char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '-'; }
		static bool IsNumTail(char c) { return std::isdigit(static_cast<unsigned char>(c)); }
	};
}