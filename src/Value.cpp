#include <JsonBox/Value.h>

#include <cassert>
#include <stack>
#include <sstream>
#include <list>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <cstddef>
#include <locale>

#include <JsonBox/Grammar.h>
#include <JsonBox/Convert.h>
#include <JsonBox/OutputFilter.h>
#include <JsonBox/Indenter.h>
#include <JsonBox/IndentCanceller.h>
#include <JsonBox/SolidusEscaper.h>
#include <JsonBox/JsonParsingError.h>
#include <JsonBox/JsonWritingError.h>

namespace JsonBox {

	/**
	 * Empty string returned by getString() when the value doesn't contain a
	 * string.
	 * @see JsonBox::Value::getString
	 */
	static const std::string EMPTY_STRING = std::string();
	
	/**
	 * Default int value returned by getInteger() when the value doesn't contain
	 * an integer.
	 * @see JsonBox::Value::getInt
	 */
	static const int EMPTY_INT = 0;

	/**
	 * More values than a std::vector can hold on any standard library: PTRDIFF_MAX / sizeof(Value) is what
	 * vector::max_size() gives with libstdc++ and MSVC, libc++ allows a little more. A constant and not
	 * Array().max_size(): that constructs a vector to ask it, once for every index that is used, and
	 * the debug library of MSVC allocates in the constructor of a vector, which is noexcept (a failed
	 * allocation ends the program there instead of throwing bad_alloc).
	 */
	static const std::size_t MAX_ARRAY_SIZE = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(Value);
	
	/**
	 * Default double value returned by getDouble() when the value doesn't
	 * contain a double.
	 * @see JsonBox::Value::getDouble
	 */
	static const double EMPTY_DOUBLE = 0.0;
	
	/**
	 * Default empty object value returned by getObject() when the value
	 * doesn't contain an object.
	 * @see JsonBox::Value::getObject
	 */
	static const Object EMPTY_OBJECT = Object();
	
	/**
	 * Default empty array value returned by getArray() when the value
	 * doesn't contain an array.
	 * @see JsonBox::Value::getArray
	 */
	static const Array EMPTY_ARRAY = Array();
	
	/**
	 * Default boolean value returned by getBoolean() when the value doesn't
	 * contain a boolean.
	 * @see JsonBox::Value::getBoolean
	 */
	static const bool EMPTY_BOOL = false;

	std::string Value::escapeMinimumCharacters(const std::string &str) {
		std::stringstream result;

		// For each character in the string.
		for (std::string::const_iterator i = str.begin(); i != str.end(); ++i) {
			if (*i == Strings::Std::QUOTATION_MARK) {
				result << Strings::Json::QUOTATION_MARK;

			} else if (*i == Strings::Std::REVERSE_SOLIDUS) {
				result << Strings::Json::REVERSE_SOLIDUS;

			} else if (*i == Strings::Std::BACKSPACE) {
				result << Strings::Json::BACKSPACE;

			} else if (*i == Strings::Std::FORM_FEED) {
				result << Strings::Json::FORM_FEED;

			} else if (*i == Strings::Std::LINE_FEED) {
				result << Strings::Json::LINE_FEED;

			} else if (*i == Strings::Std::CARRIAGE_RETURN) {
				result << Strings::Json::CARRIAGE_RETURN;

			} else if (*i == Strings::Std::TAB) {
				result << Strings::Json::TAB;

			} else if (*i >= '\0' && *i <= '\x1f') {
				result << Value::escapeToUnicode(*i);

			} else {
				result << *i;
			}
		}

		return result.str();
	}

	std::string Value::escapeAllCharacters(const std::string &str) {
		std::stringstream result;

		// For each character in the string.
		for (std::string::const_iterator i = str.begin(); i != str.end(); ++i) {
			if (*i == Strings::Std::QUOTATION_MARK) {
				result << Strings::Json::QUOTATION_MARK;

			} else if (*i == Strings::Std::REVERSE_SOLIDUS) {
				result << Strings::Json::REVERSE_SOLIDUS;

			} else if (*i == Strings::Std::SOLIDUS) {
				result << Strings::Json::SOLIDUS;

			} else if (*i == Strings::Std::BACKSPACE) {
				result << Strings::Json::BACKSPACE;

			} else if (*i == Strings::Std::FORM_FEED) {
				result << Strings::Json::FORM_FEED;

			} else if (*i == Strings::Std::LINE_FEED) {
				result << Strings::Json::LINE_FEED;

			} else if (*i == Strings::Std::CARRIAGE_RETURN) {
				result << Strings::Json::CARRIAGE_RETURN;

			} else if (*i == Strings::Std::TAB) {
				result << Strings::Json::TAB;

			} else if (*i >= '\0' && *i <= '\x1f') {
				result << Value::escapeToUnicode(*i);

			} else {
				result << *i;
			}

		}

		return result.str();
	}


	const std::string Value::escapeToUnicode(char charToEscape) {
		std::stringstream result;

		if (charToEscape >= '\0' && charToEscape <= '\x1f') {
			result << "\\u00";
			result << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(charToEscape);
		}

		return result.str();
	}

	Value::Value() : type(NULL_VALUE), data() {
	}

	Value::Value(std::istream &input) : type(NULL_VALUE), data() {
		try {
			loadFromStream(input);

		} catch (...) {
			// the exception leaves the constructor, so the destructor will not run: the part of the
			// value that was read has to be freed here
			clear();
			throw;
		}
	}

	Value::Value(const std::string &newString) : type(STRING),
		data(new std::string(newString)) {
	}

	Value::Value(const char *newCString) : type((newCString) ? (STRING) : (NULL_VALUE)),
		data((newCString) ? (new std::string(newCString)) : (static_cast<std::string *>(NULL))) {
	}

	Value::Value(int newInt) : type(INTEGER), data(new int(newInt)) {
	}

	Value::Value(double newDouble) : type(DOUBLE), data(new double(newDouble)) {
	}

	Value::Value(const Object &newObject) : type(OBJECT),
		data(new Object(newObject)) {
	}

	Value::Value(const Array &newArray) : type(ARRAY),
		data(new Array(newArray)) {
	}

	Value::Value(bool newBoolean) : type(BOOLEAN), data(new bool(newBoolean)) {
	}

	Value::Value(const Value &src) : type(src.type), data() {
		switch (type) {
		case STRING:
			data.stringValue = new std::string(*src.data.stringValue);
			break;

		case INTEGER:
			data.intValue = new int(*src.data.intValue);
			break;

		case DOUBLE:
			data.doubleValue = new double(*src.data.doubleValue);
			break;

		case OBJECT:
			data.objectValue = new Object(*src.data.objectValue);
			break;

		case ARRAY:
			data.arrayValue = new Array(*src.data.arrayValue);
			break;

		case BOOLEAN:
			data.boolValue = new bool(*src.data.boolValue);
			break;

		default:
			type = NULL_VALUE;
			break;
		}
	}

	Value::~Value() {
		clear();
	}

	Value &Value::operator=(const Value &src) {
		if (this != &src) {
			// The copy is made first: if it throws, this value is left as it was. Then the
			// payload moves over, which cannot throw.
			Value copy(src);
			clear();
			type = copy.type;
			data = copy.data;
			copy.type = NULL_VALUE;
			copy.data.stringValue = NULL;
		}

		return *this;
	}

	Value &Value::operator=(const std::string &src) {
		this->setString(src);

		return *this;
	}

	Value &Value::operator=(const char *src) {
		if (src) {
			this->setString(src);

		} else {
			this->setNull();
		}

		return *this;
	}

	Value &Value::operator=(int src) {
		this->setInteger(src);

		return *this;
	}

	Value &Value::operator=(double src) {
		this->setDouble(src);

		return *this;
	}

	Value &Value::operator=(const Object &src) {
		this->setObject(src);

		return *this;
	}

	Value &Value::operator=(const Array &src) {
		this->setArray(src);

		return *this;
	}

	Value &Value::operator=(bool src) {
		this->setBoolean(src);

		return *this;
	}

	bool Value::operator==(const Value &rhs) const {
		bool result = true;

		if (this != &rhs) {
			if (type == rhs.type) {
				switch (type) {
				case STRING:
					result = (*data.stringValue == rhs.getString());
					break;

				case INTEGER:
					result = (*data.intValue == rhs.getInteger());
					break;

				case DOUBLE:
					result = (*data.doubleValue == rhs.getDouble());
					break;

				case OBJECT:
					result = (*data.objectValue == rhs.getObject());
					break;

				case ARRAY:
					result = (*data.arrayValue == rhs.getArray());
					break;

				case BOOLEAN:
					result = (*data.boolValue == rhs.getBoolean());
					break;

				default:
					break;
				}

			} else {
				result = false;
			}
		}

		return result;
	}

	bool Value::operator!=(const Value &rhs) const {
		return !(*this == rhs);
	}

	bool Value::operator<(const Value &rhs) const {
		bool result = false;

		if (this != &rhs) {
			if (type == rhs.type) {
				switch (type) {
				case STRING:
					result = (*data.stringValue < rhs.getString());
					break;

				case INTEGER:
					result = (*data.intValue < rhs.getInteger());
					break;

				case DOUBLE:
					result = (*data.doubleValue < rhs.getDouble());
					break;

				case OBJECT:
					result = (*data.objectValue < rhs.getObject());
					break;

				case ARRAY:
					result = (*data.arrayValue < rhs.getArray());
					break;

				case BOOLEAN:
					result = (*data.boolValue < rhs.getBoolean());
					break;

				default:
					break;
				}

			}
		}

		return result;
	}

	bool Value::operator<=(const Value &rhs) const {
		return *this < rhs || *this == rhs;
	}

	bool Value::operator>(const Value &rhs) const {
		return !(*this <= rhs);
	}

	bool Value::operator>=(const Value &rhs) const {
		return *this > rhs || *this == rhs;
	}

	Value &Value::operator[](const Object::key_type &key) {
		if (type != OBJECT) {
			// allocated before clear(): if it throws, the value is left as it was
			Object *created = new Object();
			clear();
			type = OBJECT;
			data.objectValue = created;
		}

		return (*data.objectValue)[key];
	}

	Value &Value::operator[](const char *key) {
		return operator[](std::string(key));
	}

	Value &Value::operator[](Array::size_type index) {
		// index + 1 would wrap around for the largest index, and an array cannot hold that many values
		if (index >= MAX_ARRAY_SIZE) {
			throw std::out_of_range("JsonBox::Value::operator[]: the index is too large for an array");
		}

		// We make sure it's an array.
		if (type != ARRAY) {
			// allocated before clear(): if it throws, the value is left as it was
			Array *created = new Array(index + 1);
			clear();
			type = ARRAY;
			data.arrayValue = created;
		} else if (index >= (*data.arrayValue).size()) {
			// We make sure the array is big enough.
			data.arrayValue->resize(index + 1);
		}

		return (*data.arrayValue)[index];
	}

	Value::Type Value::getType() const {
		return type;
	}

	bool Value::isString() const {
		return type == STRING;
	}

	bool Value::isStringable() const {
		return type != ARRAY && type != OBJECT;
	}

	bool Value::isInteger() const {
		return type == INTEGER;
	}

	bool Value::isDouble() const {
		return type == DOUBLE;
	}

	bool Value::isNumeric() const {
		return type == INTEGER || type == DOUBLE;
	}

	bool Value::isObject() const {
		return type == OBJECT;
	}

	bool Value::isArray() const {
		return type == ARRAY;
	}

	bool Value::isBoolean() const {
		return type == BOOLEAN;
	}

	bool Value::isNull() const {
		return type == NULL_VALUE;
	}

	const std::string &Value::getString() const {
		return tryGetString(EMPTY_STRING);
	}

	const std::string &Value::tryGetString(const std::string &defaultValue) const {
		return (type == STRING) ? (*data.stringValue) : (defaultValue);
	}

	const std::string Value::getToString() const {
	switch (type) {
		case STRING:
			return *data.stringValue;

		case INTEGER: {
				std::stringstream ss;
				ss.imbue(std::locale::classic());
				ss << *data.intValue;
				return ss.str();
			}

		case DOUBLE: {
				std::stringstream ss;
				ss.imbue(std::locale::classic());
				ss << *data.doubleValue;
				return ss.str();
			}

		case BOOLEAN:
			return (*data.boolValue) ? (Literals::TRUE_STRING) : (Literals::FALSE_STRING);

		case OBJECT:
		case ARRAY: {
				std::stringstream ss;
				ss << *this;
				return ss.str();
			}

		case NULL_VALUE:
			return Literals::NULL_STRING;

		default:
			return std::string();
	}
}

	void Value::setString(std::string const &newString) {
		if (type == STRING) {
			*data.stringValue = newString;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			std::string *created = new std::string(newString);
			clear();
			type = STRING;
			data.stringValue = created;
		}
	}

	int Value::getInteger() const {
		return tryGetInteger(EMPTY_INT);
	}

	int Value::tryGetInteger(int defaultValue) const {
		if (type == INTEGER) {
			return *data.intValue;

		} else if (type == DOUBLE) {
			// converting a double that does not fit in an int (or NaN) is undefined behaviour
			const double number = *data.doubleValue;

			if (number != number) {
				return defaultValue;

			} else if (number >= 2147483648.0) {
				return std::numeric_limits<int>::max();

			} else if (number <= -2147483649.0) {
				return std::numeric_limits<int>::min();
			}

			return static_cast<int>(number);
		}

		return defaultValue;
	}

	void Value::setInteger(int newInteger) {
		if (type == INTEGER) {
			*data.intValue = newInteger;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			int *created = new int(newInteger);
			clear();
			type = INTEGER;
			data.intValue = created;
		}
	}

	double Value::getDouble() const {
		return tryGetDouble(EMPTY_DOUBLE);
	}

	double Value::tryGetDouble(double defaultValue) const {
		return (type == DOUBLE) ? (*data.doubleValue) : ((type == INTEGER) ? (static_cast<double>(*data.intValue)) : (defaultValue));
	}

	float Value::getFloat() const {
		return tryGetFloat(static_cast<float>(EMPTY_DOUBLE));
	}

	float Value::tryGetFloat(float defaultValue) const {
		return (type == DOUBLE) ? (static_cast<float>(*data.doubleValue)) : ((type == INTEGER) ? (static_cast<float>(*data.intValue)) : (defaultValue));
	}

	void Value::setDouble(double newDouble) {
		if (type == DOUBLE) {
			*data.doubleValue = newDouble;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			double *created = new double(newDouble);
			clear();
			type = DOUBLE;
			data.doubleValue = created;
		}
	}

	const Object &Value::getObject() const {
		return (type == OBJECT) ? (*data.objectValue) : (EMPTY_OBJECT);
	}

	void Value::setObject(const Object &newObject) {
		if (type == OBJECT) {
			*data.objectValue = newObject;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			Object *created = new Object(newObject);
			clear();
			type = OBJECT;
			data.objectValue = created;
		}
	}

	const Array &Value::getArray() const {
		return (type == ARRAY) ? (*data.arrayValue) : (EMPTY_ARRAY);
	}

	void Value::setArray(const Array &newArray) {
		if (type == ARRAY) {
			*data.arrayValue = newArray;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			Array *created = new Array(newArray);
			clear();
			type = ARRAY;
			data.arrayValue = created;
		}
	}

	bool Value::getBoolean() const {
		return tryGetBoolean(EMPTY_BOOL);
	}

	bool Value::tryGetBoolean(bool defaultValue) const {
		return (type == BOOLEAN) ? (*data.boolValue) : (defaultValue);
	}

	void Value::setBoolean(bool newBoolean) {
		if (type == BOOLEAN) {
			*data.boolValue = newBoolean;

		} else {
			// allocated before clear(): if it throws, the value is left as it was
			bool *created = new bool(newBoolean);
			clear();
			type = BOOLEAN;
			data.boolValue = created;
		}
	}

	void Value::setNull() {
		clear();
		type = NULL_VALUE;
		data.stringValue = NULL;
	}

	void Value::loadFromString(std::string const &json) {
		std::stringstream jsonStream(json);
		loadFromStream(jsonStream);
	}

	void Value::loadFromStream(std::istream &input) {
		readValue(input, 0);
	}

	void Value::readValue(std::istream &input, std::size_t depth) {
		char currentCharacter;

		// We check that the stream is in UTF-8. A NUL among the first two characters means UTF-16/32;
		// a character that was not read (input shorter than two characters) is not a NUL.
		char encoding[2] = {'x', 'x'};
		int probed = 0;
		if (input.get(encoding[0])) {
			++probed;
			if (input.get(encoding[1])) {
				++probed;
			}
		}
		if (probed < 2 && !input.bad()) {
			// the failed read only meant "end of input"; the characters that were read go back below
			input.clear();
		}

		if (encoding[0] != '\0' && encoding[1] != '\0') {
			// We put the characters back.
			if (probed == 2) {
				input.putback(encoding[1]);
			}
			if (probed >= 1) {
				input.putback(encoding[0]);
			}

			// Boolean value used to stop reading characters after the value
			// is done loading.
			bool reading = true;

			while (reading && input.good()) {
				input.get(currentCharacter);

				if (input.good()) {
					if (currentCharacter == Structural::BEGIN_END_STRING) {
						// The value to be parsed is a string.
						setString("");
						readString(input, *data.stringValue);
						reading = false;

					} else if (currentCharacter == Structural::BEGIN_OBJECT) {
						// The value to be parsed is an object.
						checkDepth(depth);
						setObject(Object());
						readObject(input, *data.objectValue, depth);
						reading = false;

					} else if (currentCharacter == Structural::BEGIN_ARRAY) {
						// The value to be parsed is an array.
						checkDepth(depth);
						setArray(Array());
						readArray(input, *data.arrayValue, depth);
						reading = false;

					} else if (currentCharacter == Literals::NULL_STRING[0]) {
						// We try to read the literal 'null'.
						if (!input.eof()) {
							input.get(currentCharacter);

							if (currentCharacter == Literals::NULL_STRING[1]) {
								if (!input.eof()) {
									input.get(currentCharacter);

									if (currentCharacter == Literals::NULL_STRING[2]) {
										if (!input.eof()) {
											input.get(currentCharacter);

											if (currentCharacter == Literals::NULL_STRING[3]) {
												setNull();
												reading = false;

											} else {
												throw JsonParsingError("Invalid characters found.");
											}

										} else {
											throw JsonParsingError("JSON input ends incorrectly.");
										}

									} else {
										throw JsonParsingError("Invalid characters found.");
									}

								} else {
									throw JsonParsingError("JSON ends incorrectly.");
								}

							} else {
								throw JsonParsingError("Invalid characters found");
							}

						} else {
							throw JsonParsingError("JSON input ends incorrectly.");
						}

					} else if (currentCharacter == Numbers::MINUS ||
					           (currentCharacter >= Numbers::DIGITS[0] && currentCharacter <= Numbers::DIGITS[9])) {
						// Numbers can't start with zeroes.
						input.putback(currentCharacter);
						readNumber(input, *this);
						reading = false;

					} else if (currentCharacter == Literals::TRUE_STRING[0]) {
						// We try to read the boolean literal 'true'.
						if (!input.eof()) {
							input.get(currentCharacter);

							if (currentCharacter == Literals::TRUE_STRING[1]) {
								if (!input.eof()) {
									input.get(currentCharacter);

									if (currentCharacter == Literals::TRUE_STRING[2]) {
										if (!input.eof()) {
											input.get(currentCharacter);

											if (currentCharacter == Literals::TRUE_STRING[3]) {
												setBoolean(true);
												reading = false;
											}
										}
									}
								}
							}
						}

					} else if (currentCharacter == Literals::FALSE_STRING[0]) {
						// We try to read the boolean literal 'false'.
						if (!input.eof()) {
							input.get(currentCharacter);

							if (currentCharacter == Literals::FALSE_STRING[1]) {
								if (!input.eof()) {
									input.get(currentCharacter);

									if (currentCharacter == Literals::FALSE_STRING[2]) {
										if (!input.eof()) {
											input.get(currentCharacter);

											if (currentCharacter == Literals::FALSE_STRING[3]) {
												if (!input.eof()) {
													input.get(currentCharacter);

													if (currentCharacter == Literals::FALSE_STRING[4]) {
														setBoolean(false);
														reading = false;
													}
												}
											}
										}
									}
								}
							}
						}

					} else if (!isWhiteSpace(currentCharacter)) {
						throw JsonParsingError( std::string("Invalid character found: '").append(std::string(1, currentCharacter)).append("'"));
					}
				}
			}

		} else {
			throw std::invalid_argument("JSON in input stream is not in UTF-8.");
		}
	}

	void Value::loadFromFile(const std::string &filePath) {
		std::ifstream file;
		file.open(filePath.c_str(), std::ios::binary | std::ios::in);

		if (file.is_open()) {
			loadFromStream(file);
			file.close();

		} else {
			throw std::invalid_argument(std::string("Failed to open the following JSON file: ").append(filePath));
		}
	}

	void Value::writeToStream(std::ostream &output, bool indent,
	                          bool escapeAll) const {
		this->output(output, indent, escapeAll);
	}

	void Value::writeToFile(const std::string &filePath, bool indent,
	                        bool escapeAll) const {
		std::ofstream file;
		file.open(filePath.c_str());

		if (file.is_open()) {
			writeToStream(file, indent, escapeAll);
			file.close();

		} else {
			throw JsonWritingError(std::string("Failed to open the following file to write the JSON to: ").append(filePath));
		}
	}

	Value::ValueDataPointer::ValueDataPointer(): stringValue(NULL) {
	}

	Value::ValueDataPointer::ValueDataPointer(std::string *newStringValue) :
		stringValue(newStringValue) {
	}

	Value::ValueDataPointer::ValueDataPointer(int *newIntValue) :
		intValue(newIntValue) {
	}

	Value::ValueDataPointer::ValueDataPointer(double *newDoubleValue) :
		doubleValue(newDoubleValue) {
	}

	Value::ValueDataPointer::ValueDataPointer(Object *newObjectValue) :
		objectValue(newObjectValue) {
	}

	Value::ValueDataPointer::ValueDataPointer(Array *newArrayValue) :
		arrayValue(newArrayValue) {
	}

	Value::ValueDataPointer::ValueDataPointer(bool *newBoolValue) :
		boolValue(newBoolValue) {
	}

	bool Value::isHexDigit(char digit) {
		return (digit >= Numbers::DIGITS[0] && digit <= Numbers::DIGITS[9]) || (digit >= Numbers::DIGITS[10] && digit <= Numbers::DIGITS[15]) ||
		       (digit >= Numbers::DIGITS[16] && digit <= Numbers::DIGITS[21]);
	}

	bool Value::isWhiteSpace(char whiteSpace) {
		return whiteSpace == Whitespace::SPACE ||
		       whiteSpace == Whitespace::HORIZONTAL_TAB ||
		       whiteSpace == Whitespace::NEW_LINE ||
		       whiteSpace == Whitespace::CARRIAGE_RETURN;
    }
    
    static std::string codePointToUTF8(unsigned int cp)
    {
        std::string result;
        
        // based on description from http://en.wikipedia.org/wiki/UTF-8
        
        if (cp <= 0x7f)
        {
            result.resize(1);
            result[0] = static_cast<char>(cp);
        }
        else if (cp <= 0x7FF)
        {
            result.resize(2);
            result[1] = static_cast<char>(0x80 | (0x3f & cp));
            result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
        }
        else if (cp <= 0xFFFF)
        {
            result.resize(3);
            result[2] = static_cast<char>(0x80 | (0x3f & cp));
            result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
            result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
        }
        else if (cp <= 0x10FFFF)
        {
            result.resize(4);
            result[3] = static_cast<char>(0x80 | (0x3f & cp));
            result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
            result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
            result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
        }
        
        return result;
    }
    
    static bool decodeUnicodeEscapeSequence( std::istream &input, unsigned int &unicode )
    {
        int tmpCounter = 0;
        unicode = 0;
        while (tmpCounter < 4 && input.good())
        {
            char c;
            if (!input.get(c))
                break;
            unicode *= 16;
            if ( c >= '0'  &&  c <= '9' )
                unicode += c - '0';
            else if ( c >= 'a'  &&  c <= 'f' )
                unicode += c - 'a' + 10;
            else if ( c >= 'A'  &&  c <= 'F' )
                unicode += c - 'A' + 10;
            else
            {
                std::cout << "JsonBox::error: Bad unicode escape sequence in string: hexadecimal digit expected.";
                return false;
            }
            ++tmpCounter;
        }
        if(tmpCounter < 4)
        {
            std::cout << "JsonBox::error: Bad unicode escape sequence in string.";
            return false;
        }
        return true;
    }
    
    static bool decodeUnicodeCodePoint( std::istream &input, unsigned int &unicode )
    {
        if ( !decodeUnicodeEscapeSequence( input, unicode ) )
            return false;
        if (unicode >= 0xD800 && unicode <= 0xDBFF)
        {
            // surrogate pairs
            unsigned int surrogatePair;
            char c1 = '\0', c2 = '\0';
            bool goodStart = !input.eof() && input.get(c1) && !input.eof() && input.get(c2);
            if(!goodStart)
            {
                std::cout << "JsonBox::error: expecting another \\u token to begin the second half of a unicode surrogate pair";
                return false;
            }
            
            if (c1 == '\\' && c2 == 'u' && !input.eof())
            {
                if (decodeUnicodeEscapeSequence( input, surrogatePair ))
                {
                    unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
                }
                else
                    return false;
            }
            else
            {
                std::cout << "JsonBox::error: expecting another \\u token to begin the second half of a unicode surrogate pair";
                return false;
            }
        }
        return true;
    }

	void Value::readString(std::istream &input, std::string &result) {
        bool noErrors = true;
        char currentCharacter, tmpCharacter;
        std::stringstream constructing;
        std::string tmpStr(4, ' ');
        std::stringstream tmpSs;

		// As long as there aren't any errors and that we haven't reached the
		// end of the input stream.
		while (noErrors && input.good()) {
			input.get(currentCharacter);

			if (input.good()) {
				if (currentCharacter & 0x80) { // 0x80 --> 10000000
					// The character is part of an utf8 character.
					constructing << currentCharacter;

				} else if (currentCharacter == Strings::Json::Escape::BEGIN_ESCAPE) {
					if (input.good() && input.get(tmpCharacter)) {
						switch (tmpCharacter) {
						case Strings::Json::Escape::QUOTATION_MARK:
							constructing << Strings::Std::QUOTATION_MARK;
							break;

						case Strings::Json::Escape::REVERSE_SOLIDUS:
							constructing << Strings::Std::REVERSE_SOLIDUS;
							break;

						case Strings::Json::Escape::SOLIDUS:
							constructing << Strings::Std::SOLIDUS;
							break;

						case Strings::Json::Escape::BACKSPACE:
							constructing << Strings::Std::BACKSPACE;
							break;

						case Strings::Json::Escape::FORM_FEED:
							constructing << Strings::Std::FORM_FEED;
							break;

						case Strings::Json::Escape::LINE_FEED:
							constructing << Strings::Std::LINE_FEED;
							break;

						case Strings::Json::Escape::CARRIAGE_RETURN:
							constructing << Strings::Std::CARRIAGE_RETURN;
							break;

						case Strings::Json::Escape::TAB:
							constructing << Strings::Std::TAB;
							break;

                        case Strings::Json::Escape::BEGIN_UNICODE:
                        {
                            unsigned int unicode;
                            if ( !decodeUnicodeCodePoint( input, unicode ) )
                                return;
                            constructing << codePointToUTF8(unicode);
                        }
                            
                            break;

						default:
							break;
						}
					}

				} else if (currentCharacter == '"') {
					result = constructing.str();
					noErrors = false;

				} else {
					constructing << currentCharacter;
				}
			}
		}
	}

	void Value::readObject(std::istream &input, Object &result, std::size_t depth) {
		bool noErrors = true;
		char currentCharacter = '\0';
		std::string tmpString;

		while (noErrors && input.good()) {
			input.get(currentCharacter);

			if (input.good()) {
				if (currentCharacter == Structural::BEGIN_END_STRING) {
					// We read the object's member's name.
					readString(input, tmpString);
					currentCharacter = input.peek();
					// We read white spaces until the next non white space.
					readToNonWhiteSpace(input, currentCharacter);

					if (input.good()) {

						// We make sure it's the right character.
						if (currentCharacter == Structural::NAME_SEPARATOR) {
							// We read until the value starts.
							readToNonWhiteSpace(input, currentCharacter);

							if (input.good()) {
								// We put the character back and we load the value
								// from the stream.
								input.putback(currentCharacter);
								result[tmpString].readValue(input, depth + 1);

								while (input.good() && currentCharacter != Structural::VALUE_SEPARATOR &&
								       currentCharacter != Structural::END_OBJECT) {
									input.get(currentCharacter);
								}

								if (currentCharacter == Structural::END_OBJECT) {
									// We are done reading the object.
									noErrors = false;
								}
							}
						}
					}

				} else if (currentCharacter == Structural::END_OBJECT) {
					noErrors = false;

				} else if (!isWhiteSpace(currentCharacter)) {
					std::cout << "Expected '\"', got '" << currentCharacter << "', ignoring it." << std::endl;
				}
			}
		}
	}

	void Value::readArray(std::istream &input, Array &result, std::size_t depth) {
		bool notDone = true;
		char currentChar = '\0';

		while (notDone && input.good()) {
			input.get(currentChar);

			if (input.good()) {
				if (currentChar == Structural::END_ARRAY) {
					notDone = false;

				} else if (!isWhiteSpace(currentChar)) {
					input.putback(currentChar);
					result.push_back(Value());
					result.back().type = UNKNOWN;
					result.back().readValue(input, depth + 1);

					if (result.back().type == UNKNOWN) {
						result.pop_back();
					}

					while (input.good() && currentChar != ',' &&
					       currentChar != Structural::END_ARRAY) {
						input.get(currentChar);
					}

					if (currentChar == Structural::END_ARRAY) {
						notDone = false;
					}
				}
			}
		}
	}

	void Value::readNumber(std::istream &input, JsonBox::Value &result) {
		bool notDone = true, inFraction = false, inExponent = false;
		char currentCharacter = '\0';
		std::stringstream constructing;
		// the digits are read back with >>, which must not depend on the global locale
		constructing.imbue(std::locale::classic());

		if (input.good() && input.peek() == Numbers::DIGITS[0]) {
			// We make sure there isn't more than one zero.
			input.get(currentCharacter);

			if (input.peek() == '0') {
				notDone = false;

			} else {
				input.putback(currentCharacter);
			}
		}

		while (notDone && input.get(currentCharacter)) {
			if (currentCharacter == '-') {
				if (constructing.str().empty()) {
					constructing << currentCharacter;

				} else {
					std::cout << "Expected a digit, '.', 'e' or 'E', got '" << currentCharacter << "' instead, ignoring it." << std::endl;
				}

			} else if (currentCharacter >= '0' && currentCharacter <= '9') {
				constructing << currentCharacter;

			} else if (currentCharacter == '.') {
				if (!inFraction && !inExponent) {
					inFraction = true;
					constructing << currentCharacter;
				}

			} else if (currentCharacter == 'e' || currentCharacter == 'E') {
				if (!inExponent) {
					inExponent = true;
					constructing << currentCharacter;

					if (input.good() && (input.peek() == '-' || input.peek() == '+')) {
						input.get(currentCharacter);
						constructing << currentCharacter;
					}
				}

			} else {
				input.putback(currentCharacter);
				notDone = false;
			}
		}

		if (inFraction || inExponent) {
			double doubleResult;
			constructing >> doubleResult;
			result.setDouble(doubleResult);

		} else {
			int intResult;
			constructing >> intResult;

			if (constructing.fail() && (intResult == std::numeric_limits<int>::max() || intResult == std::numeric_limits<int>::min())) {
				// The number does not fit in an int: the conversion stored the largest or the smallest int
				// and failed, which used to be what the value became. It is read as a double instead (exact up
				// to 2^53). A conversion that fails because there are no digits (a lone '-') stores 0 and is
				// not this.
				double doubleResult;
				constructing.clear();
				constructing.seekg(0);
				constructing >> doubleResult;

				// When the text is out of range for a double the standard libraries differ: libstdc++ stores the
				// largest double and libc++ infinity (which is not JSON). The largest one, with the sign, on all.
				if (doubleResult > std::numeric_limits<double>::max()) {
					doubleResult = std::numeric_limits<double>::max();

				} else if (doubleResult < -std::numeric_limits<double>::max()) {
					doubleResult = -std::numeric_limits<double>::max();
				}

				result.setDouble(doubleResult);

			} else {
				result.setInteger(intResult);
			}
		}
	}

	void Value::checkDepth(std::size_t depth) {
		// depth is the number of arrays and objects the one about to be read is inside of
		if (depth >= static_cast<std::size_t>(JSONBOX_MAX_NESTING_DEPTH)) {
			std::stringstream message;
			message << "JSON is nested too deeply: more than " << JSONBOX_MAX_NESTING_DEPTH
			        << " levels of arrays and objects.";
			throw JsonParsingError(message.str());
		}
	}

	void Value::readToNonWhiteSpace(std::istream &input, char &currentCharacter) {
		do {
			if (!input.get(currentCharacter)) {
				currentCharacter = '\0';
				return;
			}
		} while (isWhiteSpace(currentCharacter));
	}

	void Value::clear() {
		switch (type) {
		case STRING:
			delete data.stringValue;
			break;

		case INTEGER:
			delete data.intValue;
			break;

		case DOUBLE:
			delete data.doubleValue;
			break;

		case OBJECT:
			delete data.objectValue;
			break;

		case ARRAY:
			delete data.arrayValue;
			break;

		case BOOLEAN:
			delete data.boolValue;
			break;

		default:
			break;
		}
	}

namespace {
		// Installs a filter as the buffer of a stream, and puts the buffer the stream had back when it
		// goes out of scope, also when an exception passes: the filters live on the stack, a stream
		// left pointing at one is a stream pointing at freed memory. (Like rdbuf() itself, both steps
		// clear the stream's error state, as the code did before.)
		class BufferSwap {
			public:
				BufferSwap(std::ostream &newStream, std::streambuf *filter) : stream(newStream),
					original(newStream.rdbuf()) {
					stream.rdbuf(filter);
				}

				~BufferSwap() {
					stream.rdbuf(original);
				}

			private:
				BufferSwap(const BufferSwap &);
				BufferSwap &operator=(const BufferSwap &);

				std::ostream &stream;
				std::streambuf *original;
		};
	}

	void Value::output(std::ostream &output, bool indent,
	                   bool escapeAll) const {
		if (indent) {
			if (escapeAll) {
				OutputFilter<SolidusEscaper> solidusEscaper(output.rdbuf());
				BufferSwap swap(output, &solidusEscaper);
				output << *this;

			} else {
				output << *this;
			}

		} else {
			OutputFilter<IndentCanceller> indentCanceller(output.rdbuf());
			BufferSwap cancelIndentation(output, &indentCanceller);

			if (escapeAll) {
				OutputFilter<SolidusEscaper> solidusEscaper(output.rdbuf());
				BufferSwap swap(output, &solidusEscaper);
				output << *this;

			} else {
				output << *this;
			}
		}
	}

	namespace {
		// The shortest text of 15, 16 or 17 significant digits that reads back as exactly the same
		// double (17 always does). 0.5489 is written as 0.5489, not as 0.54890000000000005, and the
		// text does not depend on the locale or the flags of the stream it ends up in.
		std::string doubleToText(double value) {
			// one pair of streams per thread, building them is most of the cost of writing a number
			thread_local std::ostringstream out;
			thread_local std::istringstream in;
			thread_local bool ready = false;

			if (!ready) {
				out.imbue(std::locale::classic());
				in.imbue(std::locale::classic());
				ready = true;
			}

			std::string text;

			for (int precision = 15; precision <= 17; ++precision) {
				out.str(std::string());
				out.clear();
				out.precision(precision);
				out << value;
				text = out.str();
				in.clear();
				in.str(text);
				double readBack = 0.0;
				in >> readBack;

				if (!in.fail() && readBack == value) {
					break;
				}
			}

			return text;
		}
	}

	std::ostream &operator<<(std::ostream &output, const Value &v) {
		switch (v.type) {
		case Value::STRING:
			output << Structural::BEGIN_END_STRING << Value::escapeMinimumCharacters(v.getString()) << Structural::BEGIN_END_STRING;
			break;

		case Value::INTEGER:
			// not output << int: the locale and the flags (std::hex, std::showpos...) of the stream
			// would change the text
			output << std::to_string(v.getInteger());
			break;

		case Value::DOUBLE:
			output << doubleToText(v.getDouble());
			break;

		case Value::OBJECT:
			output << v.getObject();
			break;

		case Value::ARRAY:
			output << v.getArray();
			break;

		case Value::BOOLEAN:
			output << (v.getBoolean() ? Literals::TRUE_STRING : Literals::FALSE_STRING);
			break;

		case Value::NULL_VALUE:
			output << Literals::NULL_STRING;
			break;

		default:
			break;
		}

		return output;
	}

	std::ostream &operator<<(std::ostream &output, const Array &a) {
		if (a.empty()) {
			output << Structural::BEGIN_ARRAY << Structural::END_ARRAY;

		} else {
			output << Structural::BEGIN_ARRAY << std::endl;

			{
				OutputFilter<Indenter> indent(output.rdbuf());
				BufferSwap swap(output, &indent);

				for (Array::const_iterator i = a.begin(); i != a.end(); ++i) {
					if (i != a.begin()) {
						output << Structural::VALUE_SEPARATOR << std::endl;
					}

					output << *i;
				}
			}

			output << std::endl << Structural::END_ARRAY;
		}

		return output;
	}

	std::ostream &operator<<(std::ostream &output, const Object &o) {
		// If the object is empty, we simply write "{}".
		if (o.empty()) {
			output << Structural::BEGIN_OBJECT << Structural::END_OBJECT;

		} else {
			output << Structural::BEGIN_OBJECT << std::endl;

			{
				OutputFilter<Indenter> indent(output.rdbuf());
				BufferSwap swap(output, &indent);

				// For each item in the object.
				for (Object::const_iterator i = o.begin(); i != o.end(); ++i) {
					if (i != o.begin()) {
						output << Structural::VALUE_SEPARATOR << std::endl;
					}

					// We print the name of the attribute and its value.
					output << Structural::BEGIN_END_STRING << Value::escapeMinimumCharacters(i->first) << Structural::BEGIN_END_STRING << Whitespace::SPACE << Structural::NAME_SEPARATOR << Whitespace::SPACE << i->second;
				}
			}

			output << std::endl << Structural::END_OBJECT;
		}

		return output;
	}
}
