#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <map>
#include <vector>

namespace parse {

    static const int SPACES_PER_INDENT = 2; // spaces for one indent

    namespace token_type {
        struct Number {  // Лексема «число»
            int value;   // число
        };

        struct Id {             // Лексема «идентификатор»
            std::string value;  // Имя идентификатора
        };

        struct Char {    // Лексема «символ»
            char value;  // код символа
        };

        struct String {  // Лексема «строковая константа»
            std::string value;
        };

        struct Class {};    // Лексема «class»
        struct Return {};   // Лексема «return»
        struct If {};       // Лексема «if»
        struct Else {};     // Лексема «else»
        struct Def {};      // Лексема «def»
        struct Newline {};  // Лексема «конец строки»
        struct Print {};    // Лексема «print»
        struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
        struct Dedent {};  // Лексема «уменьшение отступа»
        struct Eof {};     // Лексема «конец файла»
        struct And {};     // Лексема «and»
        struct Or {};      // Лексема «or»
        struct Not {};     // Лексема «not»
        struct Eq {};      // Лексема «==»
        struct NotEq {};   // Лексема «!=»
        struct LessOrEq {};     // Лексема «<=»
        struct GreaterOrEq {};  // Лексема «>=»
        struct None {};         // Лексема «None»
        struct True {};         // Лексема «True»
        struct False {};        // Лексема «False»
    }  // namespace token_type

    using TokenBase
        = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
        token_type::Class, token_type::Return, token_type::If, token_type::Else,
        token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
        token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
        token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
        token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
        [[nodiscard]] const Token& CurrentToken() const;

        // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
        Token NextToken();

        // Если текущий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& Expect() const {
            using namespace std::literals;
            // Заглушка. Реализуйте метод самостоятельно
            if (!(*current_token_it_).Is<T>())
            {
                throw LexerError("Token::Expect() method has failed."s);
            }
            return CurrentToken().As<T>();
        }

        // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void Expect(const U& value) const {
            using namespace std::literals;          
            Token other_token(T{ value }); // делаем из value токен типа T          
            if (*current_token_it_ != other_token)
            {
                throw LexerError("Token::Expect(value) method has failed."s);
            }
        }

        // Если следующий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& ExpectNext() {
            using namespace std::literals;
            NextToken();
            return Expect<T>();
        }

        // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value) {
            using namespace std::literals;
            NextToken();
            Expect<T>(value);
        }

    private:
       
        int global_indent_counter_ = 0;
        const std::map<std::string, Token> keyword_map_ = 
        {
            {std::string{"class"},token_type::Class{}},
            {std::string{"return"},token_type::Return{}},
            {std::string{"if"},token_type::If{}},
            {std::string{"else"},token_type::Else{}},
            {std::string{"def"},token_type::Def{}},
            {std::string{"print"},token_type::Print{}},
            {std::string{"and"},token_type::And{}},
            {std::string{"or"},token_type::Or{}},
            {std::string{"not"},token_type::Not{}},
            {std::string{"None"},token_type::None{}},
            {std::string{"True"},token_type::True{}},
            {std::string{"False"},token_type::False{}}
        };

        std::vector<Token> tokens_; //разобранные токены
        std::vector<Token>::const_iterator current_token_it_; // итератор не текущий токен
        const std::istream& in_stream_;// входной поток для разбора

        void TrimSpaces(std::istream& istr);//обрезание пустых пробелов
        void ParseInputStream(std::istream& istr); //парсим входной поток

        //----------- Методы парсинга данных/токенов----------
        void ParseIndent(std::istream& istr);
        void ParseString(std::istream& istr);
        void ParseKeywords(std::istream& istr);
        void ParseChars(std::istream& istr);
        void ParseNumbers(std::istream& istr);
        void ParseNewLine(std::istream& istr);
        void ParseComments(std::istream& istr);       
    
    };

} // namespace parse