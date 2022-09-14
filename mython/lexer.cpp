#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <cctype>


using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input):in_stream_(input)
    {
        ParseInputStream(input);
       
    }

    const Token& Lexer::CurrentToken() const {
      
        return *current_token_it_;
    }

    Token Lexer::NextToken() {
        if ((current_token_it_ + 1) == tokens_.end())
        {          
            return *current_token_it_;
        }
        return *(++current_token_it_);
    }

    void Lexer::TrimSpaces(std::istream& input)
    {
        while (input.peek() == ' ')
        {
            input.get();
        }
    }

    void Lexer::ParseIndent(std::istream& input) {
        if (input.peek() == std::char_traits<char>::eof())
        {
            return;
        }
        char ch;
        int spaces = 0;
        while (input.get(ch) && (ch == ' '))
        {
            ++spaces;
        }

        if (input.rdstate() != std::ios_base::eofbit)
        {
            input.putback(ch);
            if (ch == '\n')
            {
                return;
            }
        }
        //если пробелов больше чем было нужен индент
        if (spaces > global_indent_counter_ * SPACES_PER_INDENT )
        {

            spaces -= global_indent_counter_ * SPACES_PER_INDENT; //сколько пробелов сверх предыдущего нидента
            while (spaces > 0)
            {
                spaces -= SPACES_PER_INDENT;
                tokens_.emplace_back(token_type::Indent{}); // довобляем пробелы в индент
                ++global_indent_counter_;
            }
        } 
        //если пробелов меньше чем было нужен дедент
        else if (spaces < global_indent_counter_ * SPACES_PER_INDENT)
        {
            spaces = global_indent_counter_ * SPACES_PER_INDENT - spaces;
            while (spaces > 0)
            {
                spaces -= SPACES_PER_INDENT;
                tokens_.emplace_back(token_type::Dedent{});
                --global_indent_counter_;
            }
        }

    }

    void Lexer::ParseComments(std::istream& input)
    {
        char ch = input.peek();
        if (ch == '#')
        {
            std::string tmp_str;
            std::getline(input, tmp_str, '\n');

            // Вернем перевод строки \n, если только это не конец потока
            // (в конце потока \n игнорируется)
            if (input.rdstate() != std::ios_base::eofbit)
            {
                input.putback('\n');
            }
        }
    }


    void Lexer::ParseInputStream(std::istream& input) {
        global_indent_counter_ = 0;
        tokens_.clear();
        current_token_it_ = tokens_.begin();
        TrimSpaces(input);
        while (input) {
            ParseString(input);
            ParseKeywords(input);
            ParseChars(input);
            ParseNumbers(input);
            TrimSpaces(input);
            ParseNewLine(input);
        }
        // перед Eof должен всешда быть Newline
        if (!tokens_.empty() && (!tokens_.back().Is<token_type::Newline>()))
        {
            tokens_.emplace_back(token_type::Newline{});
        }

        // убираем остатки отступов
        while (global_indent_counter_ > 0)
        {
            tokens_.emplace_back(token_type::Dedent{});
            --global_indent_counter_;
        }
        tokens_.emplace_back(token_type::Eof{});
        current_token_it_ = tokens_.begin();
    }

    void Lexer::ParseString(std::istream& input)
    {
        if (input.peek() == std::char_traits<char>::eof())
        {
            return;
        }

        char open_char = input.get();

        // Если открывающий символ кавычка, то это строка
        if ((open_char == '\'') || (open_char == '\"'))
        {
            char ch;
            std::string result;
            // Читаем символы пока не встретим закрывающий символ
            while (input.get(ch))
            {
                if (ch == open_char)
                {
                    // Найден закрывающий символ
                    break;
                }
                else if (ch == '\\')
                {
                    // Ищем экранированный символ
                    char esc_ch;
                    if (input.get(esc_ch))
                    {
                        // Все допустимые esc-символы добавляем к результату
                        switch (esc_ch)
                        {
                        case 'n':
                            result.push_back('\n');
                            break;
                        case 't':
                            result.push_back('\t');
                            break;
                        case 'r':
                            result.push_back('\r');
                            break;
                        case '"':
                            result.push_back('"');
                            break;
                        case '\'':
                            result.push_back('\'');
                            break;
                        case '\\':
                            result.push_back('\\');
                            break;
                        default:
                            throw std::logic_error("ParseString() has encountered unknown escape sequence \\"s + esc_ch);
                        }
                    }
                    else
                    {
                        // Ошибка. Неожиданный конец потока
                        using namespace std::literals;
                        throw LexerError("ParseString() has encountered unexpected end of stream after a backslash"s);
                    }
                }
                else if ((ch == '\n') || (ch == '\r'))
                {
                    // Ошибка. Недопустимый символ перевода строки или возврата каретки
                    using namespace std::literals;
                    throw LexerError("ParseString() has encountered NL or CR symbol within a string"s);
                }
                else
                {
                    result.push_back(ch);
                }
            }
            //Проверяем на закрытие
            if (open_char == ch)
            {
                // Закрыли
                tokens_.emplace_back(token_type::String{ result });
            }
            else
            {
                // Не закрыли
                using namespace std::literals;
                throw LexerError("ParseString() has exited without find end-of-string character"s);
            }
        }
        else
        {
            // не строка, возвращаем символ
            input.putback(open_char);
        }
    }

    void Lexer::ParseKeywords(std::istream& input)
    {
        if (input.peek() == std::char_traits<char>::eof())
        {
            return;
        }

        char ch = input.peek();

        // Ключевые слова и идентификаторы должны начинаться с букв или подчеркивания
        if (std::isalpha(ch) || ch == '_')
        {
            std::string keyword;
            while (input.get(ch))
            {
                if (std::isalnum(ch) || ch == '_') //isalnum - alpha/numeric check
                {
                    keyword.push_back(ch);
                }
                else
                {
                    input.putback(ch);
                    break;
                }
            }

            // Добавляем полученый keyword в вектор токенов
            if (keyword_map_.find(keyword) != keyword_map_.end())
            {
                tokens_.push_back(keyword_map_.at(keyword));
            }
            else
            {
                tokens_.emplace_back(token_type::Id{ keyword });
            }
        }
    }

    void Lexer::ParseChars(std::istream& input)
    {
        if (input.peek() == std::char_traits<char>::eof())
        {
            return;
        }

        char ch;
        input.get(ch);

        // Обрабатываем только символы пунктуации
        if (std::ispunct(ch))
        {
            if (ch == '#')// коментарий
            {              
                input.putback(ch); //кладем обратно и парсим комменты
                ParseComments(input);
                return;
            }
            else if ((ch == '=') && (input.peek() == '='))
            {
                // Двойной символ ==
                input.get();
                tokens_.emplace_back(token_type::Eq{});
            }
            else if ((ch == '!') && (input.peek() == '='))
            {
                // Двойной символ !=
                input.get();
                tokens_.emplace_back(token_type::NotEq{});
            }
            else if ((ch == '>') && (input.peek() == '='))
            {
                // Двойной символ >=
                input.get();
                tokens_.emplace_back(token_type::GreaterOrEq{});
            }
            else if ((ch == '<') && (input.peek() == '='))
            {
                // Двойной символ <=
                input.get();
                tokens_.emplace_back(token_type::LessOrEq{});
            }
            else
            {
                // Это одинарный символ
                tokens_.emplace_back(token_type::Char{ ch });
            }
        }
        else
        {
            // что то не-то не берем
            input.putback(ch);
        }
    }

    void Lexer::ParseNumbers(std::istream& input)
    {
        if (input.peek() == std::char_traits<char>::eof())
        {
            return;
        }

        char ch = input.peek();

        // Обрабатываем только цифры
        if (std::isdigit(ch))
        {
            std::string result;
            while (input.get(ch))
            {
                if (std::isdigit(ch))
                {
                    result.push_back(ch);
                }
                else
                {
                    // Вернем не цифру
                    input.putback(ch);
                    break;
                }
            }
            // Числа в Mython только int
            int num = std::stoi(result);
            tokens_.emplace_back(token_type::Number{ num });
        }
    }

    void Lexer::ParseNewLine(std::istream& input)
    {
        char ch = input.peek();

        if (ch == '\n')
        {
            input.get(ch);

            // В векторе токенов может быть только 1 новая строка подряд
            if (!tokens_.empty() && (!tokens_.back().Is<token_type::Newline>()))
            {
                tokens_.emplace_back(token_type::Newline{});
            }

            // Проверка отступа 
            ParseIndent(input);
        }
    }





}  // namespace parse