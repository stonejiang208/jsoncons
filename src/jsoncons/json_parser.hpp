// Copyright 2015 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://sourceforge.net/projects/jsoncons/files/ for latest version
// See https://sourceforge.net/p/jsoncons/wiki/Home/ for documentation.

#ifndef JSONCONS_JSON_PARSER_HPP
#define JSONCONS_JSON_PARSER_HPP

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <istream>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#include "jsoncons/jsoncons.hpp"
#include "jsoncons/json_input_handler.hpp"
#include "jsoncons/parse_error_handler.hpp"

namespace jsoncons {

namespace mode {
    enum mode_t {
        array,
        done,
        key,
        object
    };
};

namespace state {
    enum state_t {
        start, 
        ok,  
        object,
        key, 
        colon,
        value,
        array, 
        string,
        escape, 
        u1, 
        u2, 
        u3, 
        u4, 
        surrogate_pair, 
        u5, 
        u6, 
        u7, 
        u8, 
        u9, 
        minus, 
        zero,  
        integer,
        fraction,
        exp1,
        exp2,
        exp3,
        t,  
        tr,  
        tru, 
        f,  
        fa, 
        fal,
        fals,
        n,
        nu,
        nul,
        slash,  
        slash_slash, 
        slash_star, 
        slash_star_star,
        done 
    };
};

template<typename Char>
class basic_json_reader : private basic_parsing_context<Char>
{
    struct stack_item
    {
        stack_item()
           : mode(0),
             minimum_structure_capacity(0)
        {
        }
        int mode;
        size_t minimum_structure_capacity;
    };
    static const size_t default_max_buffer_length = 16384;
public:
    basic_json_reader(std::basic_istream<Char>& is,
                      basic_json_input_handler<Char>& handler)
       : top_(-1),
         stack_(100),
         is_(std::addressof(is)),
         handler_(std::addressof(handler)),
         err_handler_(std::addressof(default_basic_parse_error_handler<Char>::instance())),
         buffer_capacity_(default_max_buffer_length),
         is_negative_(false),
         cp_(0),
         eof_(false) 

    {
        state_ = state::start;
        this->depth_ = 200;
    }

    basic_json_reader(std::basic_istream<Char>& is,
                      basic_json_input_handler<Char>& handler,
                      basic_parse_error_handler<Char>& err_handler)
       : top_(-1),
         stack_(100),
         is_(std::addressof(is)),
         handler_(std::addressof(handler)),
         err_handler_(std::addressof(err_handler)),
         buffer_capacity_(default_max_buffer_length),
         is_negative_(false),
         cp_(0),
         eof_(false)

    {
        state_ = state::start;
        this->depth_ = 200;
    }

    ~basic_json_reader()
    {
    }

    void end_frac_value()
    {
        try
        {
            double d = string_to_float(string_buffer_);
            if (is_negative_)
                d = -d;
            handler_->value(d, *this);
        }
        catch (...)
        {
            err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_parser_category()), *this);
            handler_->value(null_type(), *this);
        }
        string_buffer_.clear();
        is_negative_ = false;
        state_ = state::ok;
    }

    void end_integer_value()
    {
        if (is_negative_)
        {
            try
            {
                long long d = string_to_integer(is_negative_, string_buffer_.c_str(), string_buffer_.length());
                handler_->value(d, *this);
            }
            catch (const std::exception&)
            {
                try
                {
                    double d = string_to_float(string_buffer_);
                    handler_->value(-d, *this);
                }
                catch (...)
                {
                    err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_parser_category()), *this);
                    handler_->value(null_type(), *this);
                }
            }
        }
        else
        {
            try
            {
                unsigned long long d = string_to_unsigned(string_buffer_.c_str(), string_buffer_.length());
                handler_->value(d, *this);
            }
            catch (const std::exception&)
            {
                try
                {
                    double d = string_to_float(string_buffer_);
                    handler_->value(d, *this);
                }
                catch (...)
                {
                    err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_parser_category()), *this);
                    handler_->value(null_type(), *this);
                }
            }
        }
        string_buffer_.clear();
        is_negative_ = false;
        state_ = state::ok;
    }

    void append_codepoint(int next_char)
    {
        switch (next_char)
        {
        case '0': 
        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':case 'B':case 'C':case 'D':case 'F':
        case 'E':
            cp_ = append_to_codepoint(cp_, next_char);
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
            break;
        }
    }

    void append_second_codepoint(int next_char)
    {
        switch (next_char)
        {
        case '0': 
        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':case 'B':case 'C':case 'D':case 'F':
        case 'E':
            cp2_ = append_to_codepoint(cp2_, next_char);
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
            break;
        }
    }

    void escape_next_char(int next_input)
    {
        switch (next_input)
        {
        case '\"':
            string_buffer_.push_back('\"');
            state_ = state::string;
            break;
        case '\\': 
            string_buffer_.push_back('\\');
            state_ = state::string;
            break;
        case '/':
            string_buffer_.push_back('/');
            state_ = state::string;
            break;
        case 'b':
            string_buffer_.push_back('\b');
            state_ = state::string;
            break;
        case 'f':  
            string_buffer_.push_back('\f');
            state_ = state::string;
            break;
        case 'n':
            string_buffer_.push_back('\n');
            state_ = state::string;
            break;
        case 'r':
            string_buffer_.push_back('\r');
            state_ = state::string;
            break;
        case 't':
            string_buffer_.push_back('\t');
            state_ = state::string;
            break;
        case 'u':
            cp_ = 0;
            state_ = state::u1;
            break;
        default:    
            err_handler_->error(std::error_code(json_parser_errc::illegal_escaped_character, json_parser_category()), *this);
            break;
        }
    }

    void end_string_value() // " -4 
    {
        switch (stack_[top_].mode)
        {
        case mode::key:
            handler_->name(string_buffer_.c_str(), string_buffer_.length(), *this);
            string_buffer_.clear();
            state_ = state::colon;
            break;
        case mode::array:
        case mode::object:
            handler_->value(string_buffer_.c_str(), string_buffer_.length(), *this);
            string_buffer_.clear();
            state_ = state::ok;
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
            break;
        }
        string_buffer_.clear();
        is_negative_ = false;
    }

    void flip_object_key() // , -3
    {
        switch (stack_[top_].mode)
        {
        case mode::object:
            // A comma causes a flip from object mode to key mode.
            if (!flip(mode::object, mode::key))
            {
                err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
            }
            //handler_->end_object(*this);
            state_ = state::key;
            break;
        case mode::array:
            state_ = state::value;
            break;
        default:
            err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
            break;
        }
    }

    void flip_key_object()
    {
        if (!flip(mode::key, mode::object))
        {
            err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
        }
        state_ = state::value;
    }
 
    uint32_t append_to_codepoint(uint32_t cp, int next_char)
    {
        cp *= 16;
        if (next_char >= '0'  &&  next_char <= '9')
        {
            cp += next_char - '0';
        }
        else if (next_char >= 'a'  &&  next_char <= 'f')
        {
            cp += next_char - 'a' + 10;
        }
        else if (next_char >= 'A'  &&  next_char <= 'F')
        {
            cp += next_char - 'A' + 10;
        }
        else
        {
            err_handler_->error(std::error_code(json_parser_errc::invalid_hex_escape_sequence, json_parser_category()), *this);
        }
        return cp;
    }

    bool eof() const
    {
        return eof_;
    }

    unsigned long do_line_number() const override
    {
        return line_;
    }

    unsigned long do_column_number() const override
    {
        return column_;
    }

    bool do_eof() const override
    {
        return eof_;
    }

    Char do_last_char() const override
    {
        return c_;
    }

    size_t do_minimum_structure_capacity() const override
    {
        return top_ >= 0 ? stack_[top_].minimum_structure_capacity : 0;
    }

    int push(int mode)
    {
        ++top_;
        if (top_ >= depth_)
        {
            depth_ *= 2;
            stack_.resize(depth_);
        }
        stack_[top_].mode = mode;
        return true;
    }

    int flip(int mode1, int mode2)
    {
        if (top_ < 0 || stack_[top_].mode != mode1)
        {
            return false;
        }
        stack_[top_].mode = mode2;
        return true;
    }

    int pop(int mode)
    {
        if (top_ < 0 || stack_[top_].mode != mode)
        {
            return false;
        }
        --top_;
        return true;
    }

    template<typename Char>
    unsigned long long string_to_unsigned(const Char *s, size_t length) throw(std::overflow_error)
    {
        const unsigned long long max_value = std::numeric_limits<unsigned long long>::max JSONCONS_NO_MACRO_EXP();
        const unsigned long long max_value_div_10 = max_value / 10;
        unsigned long long n = 0;
        for (size_t i = 0; i < length; ++i)
        {
            unsigned long long x = s[i] - '0';
            if (n > max_value_div_10)
            {
                throw std::overflow_error("Unsigned overflow");
            }
            n = n * 10;
            if (n > max_value - x)
            {
                throw std::overflow_error("Unsigned overflow");
            }

            n += x;
        }
        return n;
    }

    template<typename Char>
    long long string_to_integer(bool has_neg, const Char *s, size_t length) throw(std::overflow_error)
    {
        const long long max_value = std::numeric_limits<long long>::max JSONCONS_NO_MACRO_EXP();
        const long long max_value_div_10 = max_value / 10;

        long long n = 0;
        for (size_t i = 0; i < length; ++i)
        {
            long long x = s[i] - '0';
            if (n > max_value_div_10)
            {
                throw std::overflow_error("Integer overflow");
            }
            n = n * 10;
            if (n > max_value - x)
            {
                throw std::overflow_error("Integer overflow");
            }

            n += x;
        }
        return has_neg ? -n : n;
    }

    void read()
    {
        state_ = state::start;
        top_ = -1;
        line_ = 1;
        column_ = 0;
        prev_char_ = 0;
        eof_ = false;
        handler_->begin_json();
        buffer_.resize(buffer_capacity_);
        while (!eof_ && state_ != state::done)
        {
            if (!is_->eof())
            {
                is_->read(&buffer_[0], buffer_capacity_);
                buffer_length_ = static_cast<size_t>(is_->gcount());
                if (buffer_length_ == 0)
                {
                    eof_ = true;
                }
            }
            else
            {
                eof_ = true;
            }
            if (!eof_)
            {
                read_buffer();
            }
        }
        if (top_ >= 0)
        {
            err_handler_->error(std::error_code(json_parser_errc::unexpected_eof, json_parser_category()), *this);
        }
        //check_done();
        handler_->end_json();
    }

    void read_buffer()
    {
        for (size_t i = 0; i < buffer_length_ && state_ != state::done; ++i)
        {
            int next_char = buffer_[i];
            switch (next_char)
            {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x0b:
            case 0x0c:
            case 0x0e:
            case 0x0f:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1a:
            case 0x1b:
            case 0x1c:
            case 0x1d:
            case 0x1e:
            case 0x1f:
                err_handler_->error(std::error_code(json_parser_errc::illegal_control_character, json_parser_category()), *this);
                break;
            case '\r':
                ++line_;
                column_ = 0;
                break;
            case '\n':
                if (prev_char_ != '\r')
                {
                    ++line_;
                }
                column_ = 0;
                break;
            }
            ++column_;

            switch (state_)
            {
            case state::start: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break; // No change
                case '{':
                    if (!push(mode::key))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::object;
                    //count_members(i+1);
                    handler_->begin_object(*this);
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::array;
                    count_members(i + 1);
                    handler_->begin_array(*this);
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                case '}':
                    err_handler_->error(std::error_code(json_parser_errc::unexpected_end_of_object, json_parser_category()), *this);
                    break;
                case ']':
                    err_handler_->error(std::error_code(json_parser_errc::unexpected_end_of_array, json_parser_category()), *this);
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    break;
                }
                break;

            case state::ok: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break; // No change
                case '}':
                    if (!pop(mode::object))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ']':
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ',':
                    flip_object_key();
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::invalid_number, json_parser_category()), *this);
                    break;
                }
                break;
            case state::object: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '}':
                    if (!pop(mode::key))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_name, json_parser_category()), *this);
                    break;
                }
                break;
            case state::key: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_name, json_parser_category()), *this);
                    break;
                }
                break;
            case state::colon: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case ':':
                    flip_key_object();
                    state_ = state::value;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_name_separator, json_parser_category()), *this);
                    break;
                }
                break;
            case state::value: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '{':
                    if (!push(mode::key))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::object;
                    //count_members(i+1);
                    handler_->begin_object(*this);
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::array;
                    count_members(i + 1);
                    handler_->begin_array(*this);
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                case '-':
                    is_negative_ = true;
                    state_ = state::minus;
                    break;
                case '0': 
                    string_buffer_.push_back(next_char);
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::integer;
                    break;
                case 'f':
                    state_ = state::f;
                    break;
                case 'n':
                    state_ = state::n;
                    break;
                case 't':
                    state_ = state::t;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::array: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '{':
                    if (!push(mode::key))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::object;
                    //count_members(i+1);
                    handler_->begin_object(*this);
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    state_ = state::array;
                    count_members(i + 1);
                    handler_->begin_array(*this);
                    break;
                case ']':
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                case '-':
                    is_negative_ = true;
                    state_ = state::minus;
                    break;
                case '0': 
                    string_buffer_.push_back(next_char);
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::integer;
                    break;
                case 'f':
                    state_ = state::f;
                    break;
                case 'n':
                    state_ = state::n;
                    break;
                case 't':
                    state_ = state::t;
                    break;

                case '}':
                    err_handler_->error(std::error_code(json_parser_errc::unexpected_end_of_object, json_parser_category()), *this);
                    break;
                case ':':
                    break;
                case ',':
                    err_handler_->error(std::error_code(json_parser_errc::unexpected_value_separator, json_parser_category()), *this);
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::string: 
                switch (next_char)
                {
                case '\n':
                case '\r':
                case '\t':
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                case '\\': 
                    state_ = state::escape;
                    break;
                case '\"':
                    end_string_value();
					break;
                default:
                    string_buffer_.push_back(next_char);
                    break;
                }
                break;
            case state::escape: 
                escape_next_char(next_char);
                break;
            case state::u1: 
                append_codepoint(next_char);
                state_ = state::u2;
                break;
            case state::u2: 
                append_codepoint(next_char);
                state_ = state::u3;
                break;
            case state::u3: 
                append_codepoint(next_char);
                state_ = state::u4;
                break;
            case state::u4: 
                append_codepoint(next_char);
                if (cp_ >= min_lead_surrogate && cp_ <= max_lead_surrogate)
                {
                    state_ = state::surrogate_pair;
                }
                else
                {
                    json_char_traits<Char, sizeof(Char)>::append_codepoint_to_string(cp_, string_buffer_);
                    state_ = state::string;
                }
                break;
            case state::surrogate_pair: 
                switch (next_char)
                {
                case '\\': 
                    cp2_ = 0;
                    state_ = state::u5;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::u5: 
                switch (next_char)
                {
                case 'u':
                    state_ = state::u6;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::u6:
                append_second_codepoint(next_char);
                state_ = state::u7;
                break;
            case state::u7: 
                append_second_codepoint(next_char);
                state_ = state::u8;
                break;
            case state::u8: 
                append_second_codepoint(next_char);
                state_ = state::u9;
                break;
            case state::u9: 
				{
                    append_second_codepoint(next_char);
                    uint32_t cp = 0x10000 + ((cp_ & 0x3FF) << 10) + (cp2_ & 0x3FF);
                    json_char_traits<Char, sizeof(Char)>::append_codepoint_to_string(cp, string_buffer_);
                    state_ = state::string;
				}
                break;
            case state::minus:  
                switch (next_char)
                {
                case '0': 
                    string_buffer_.push_back(next_char);
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::integer;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::zero:  
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    end_integer_value();
                    state_ = state::ok;
                    break; // No change
                case '}':
                    end_integer_value();
                    if (!pop(mode::object))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ']':
                    end_integer_value();
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case '.':
                    string_buffer_.push_back(next_char);
                    state_ = state::fraction;
                    break;
                case ',':
                    end_integer_value();
                    flip_object_key();
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::integer: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    end_integer_value();
                    state_ = state::ok;
                    break; 
                case '}':
                    end_integer_value();
                    if (!pop(mode::object))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ']':
                    end_integer_value();
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::integer;
                    break;
                case '.':
                    string_buffer_.push_back(next_char);
                    state_ = state::fraction;
                    break;
                case ',':
                    end_integer_value();
                    flip_object_key();
                    break;
                case 'e':
                case 'E':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp1;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::fraction: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    end_frac_value();
                    state_ = state::ok;
                    break; 
                case '}':
                    end_frac_value();
                    if (!pop(mode::object))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ']':
                    end_frac_value();
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::fraction;
                    break;
                case ',':
                    end_frac_value();
                    flip_object_key();
                    break;
                case 'e':
                case 'E':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp1;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::exp1: 
                switch (next_char)
                {
                case '+':
                    state_ = state::exp2;
                    break;
                case '-':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp2;
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp3;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::exp2:  
                switch (next_char)
                {
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp3;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::exp3: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    end_frac_value();
                    state_ = state::ok;
                    break; 
                case '}':
                    end_frac_value();
                    if (!pop(mode::object))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_object(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ']':
                    end_frac_value();
                    if (!pop(mode::array))
                    {
                        err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    }
                    handler_->end_array(*this);
                    state_ = top_ == -1 ? state::done : state::ok;
                    break;
                case ',':
                    end_frac_value();
                    flip_object_key();
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    string_buffer_.push_back(next_char);
                    state_ = state::exp3;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::t: 
                switch (next_char)
                {
                case 'r':
                    state_ = state::tr;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::tr: 
                switch (next_char)
                {
                case 'u':
                    state_ = state::tru;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::tru:  
                switch (next_char)
                {
                case 'e': 
                    handler_->value(true, *this);
                    state_ = state::ok;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::f:  
                switch (next_char)
                {
                case 'a':
                    state_ = state::fa;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::fa: 
                switch (next_char)
                {
                case 'l': 
                    state_ = state::fal;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::fal:  
                switch (next_char)
                {
                case 's':
                    state_ = state::fals;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::fals:  
                switch (next_char)
                {
                case 'e':
                    handler_->value(false, *this);
                    state_ = state::ok;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::n: 
                switch (next_char)
                {
                case 'u':
                    state_ = state::nu;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::nu:  
                switch (next_char)
                {
                case 'l': 
                    state_ = state::nul;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::nul:  
                switch (next_char)
                {
                case 'l': 
                    handler_->value(null_type(), *this);
                    state_ = state::ok;
                    break;
                default:
                    err_handler_->error(std::error_code(json_parser_errc::expected_value, json_parser_category()), *this);
                    break;
                }
                break;
            case state::slash: 
                switch (next_char)
                {
                case '*':
                    state_ = state::slash_star;
                    break;
                case '/':
                    state_ = state::slash_slash;
                    break;
                default:    
                    err_handler_->error(std::error_code(json_parser_errc::expected_container, json_parser_category()), *this);
                    break;
                }
                break;
            case state::slash_star:  
                switch (next_char)
                {
                case '*':
                    state_ = state::slash_star_star;
                    break;
                }
                break;
            case state::slash_slash: 
                switch (next_char)
                {
                case '\n':
                case '\r':
                    state_ = saved_state_;
                    break;
                }
                break;
            case state::slash_star_star: 
                switch (next_char)
                {
                case '/':
                    state_ = saved_state_;
                    break;
                default:    
                    state_ = state::slash_star;
                    break;
                }
                break;
            }
            prev_char_ = next_char;
        }
    }

    void count_members(size_t start_index)
    {
        int start_top = top_;
        int start_state = state_;
        int start_saved_state = saved_state_;
        int start_mode = stack_[top_].mode;

        stack_[top_].minimum_structure_capacity = 0;

        bool done = false;
        for (size_t i = start_index; !done && i < buffer_length_; ++i)
        {
            int next_char = buffer_[i];

            switch (state_)
            {
            case state::start:  
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break; 
                case '{':
                    if (!push(mode::key))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::object;
                    }
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::array;
                    }
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;

            case state::ok: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break; 
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::object))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ',':
                    flip_object_key();
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    done = true; 
                    break;
                }
                break;
            case state::object:
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::key))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::key: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::colon:  
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case ':':
                    flip_key_object();
                    state_ = state::value;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::value:  
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '{':
                    if (!push(mode::key))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::object;
                    }
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::array;
                    }
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                case '-':
                    state_ = state::minus;
                    break;
                case '0': 
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::integer;
                    break;
                case 'f':
                    state_ = state::f;
                    break;
                case 'n':
                    state_ = state::n;
                    break;
                case 't':
                    state_ = state::t;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::array: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;
                case '{':
                    if (!push(mode::key))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::object;
                    }
                    break;
                case '[':
                    if (!push(mode::array))
                    {
                        done = true;
                    }
                    else
                    {
                        state_ = state::array;
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case '\"':
                    state_ = state::string;
                    break;
                case '/':
                    saved_state_ = state_;
                    state_ = state::slash;
                    break;
                case '-':
                    state_ = state::minus;
                    break;
                case '0': 
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::integer;
                    break;
                case 'f':
                    state_ = state::f;
                    break;
                case 'n':
                    state_ = state::n;
                    break;
                case 't':
                    state_ = state::t;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::string: 
                switch (next_char)
                {
                case '\n':
                case '\r':
                case '\t':
                    done = true; // Error
                    break;
                case '\\': 
                    state_ = state::escape;
                    break;
                case '\"':
                    switch (stack_[top_].mode)
                    {
                    case mode::key:
                        state_ = state::colon;
                        break;
                    case mode::array:
                    case mode::object:
                        if (top_ == start_top)
                        {
                            ++stack_[start_top].minimum_structure_capacity;
                        }
                        state_ = state::ok;
                        break;
                    default:
                        done = true; // Error
                        break;
                    }
					break;
                default:
                    break;
                }
                break;
            case state::escape: 
                switch (next_char)
                {
                case '\"':
                case '\\': 
                case '/':
                case 'b':
                case 'f':  
                case 'n':
                case 'r':
                case 't':
                    state_ = state::string;
                    break;
                case 'u':
                    state_ = state::u1;
                    break;
                default:   
                    done = true; // Error
                    break;
                }
                break;
            case state::u1:  
                state_ = state::u2;
                break;
            case state::u2:  
                state_ = state::u3;
                break;
            case state::u3:  
                state_ = state::u4;
                break;
            case state::u4:  
                state_ = state::string;
                break;
            case state::minus:
                switch (next_char)
                {
                case '0': 
                    state_ = state::zero;
                    break;
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::integer;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::zero: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break; 
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::object))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case '.':
                    state_ = state::fraction;
                    break;
                case ',':
                    flip_object_key();
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::integer: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break; 
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::object))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::integer;
                    break;
                case '.':
                    state_ = state::fraction;
                    break;
                case ',':
                    flip_object_key();
                    break;
                case 'e':
                case 'E':
                    state_ = state::exp1;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::fraction: 
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break; 
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::object))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::fraction;
                    break;
                case ',':
                    flip_object_key();
                    break;
                case 'e':
                case 'E':
                    state_ = state::exp1;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::exp1:
                switch (next_char)
                {
                case '+':
                    state_ = state::exp2;
                    break;
                case '-':
                    state_ = state::exp2;
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::exp3;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::exp2:  
                switch (next_char)
                {
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::exp3;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::exp3:  
                switch (next_char)
                {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break; // No change
                case '}':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::object))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ']':
                    if (top_ == start_top)
                    {
                        done = true;
                    }
                    else
                    {
                        if (!pop(mode::array))
                        {
                            done = true;
                        }
                        else
                        {
                            state_ = state::ok;
                            if (top_ == start_top)
                            {
                                ++stack_[start_top].minimum_structure_capacity;
                            }
                        }
                    }
                    break;
                case ',':
                    flip_object_key();
                    break;
                case '0': 
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8': case '9':
                    state_ = state::exp3;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::t: 
                switch (next_char)
                {
                case 'r':
                    state_ = state::tr;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::tr: 
                switch (next_char)
                {
                case 'u':
                    state_ = state::tru;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::tru:  
                switch (next_char)
                {
                case 'e': 
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::f:  
                switch (next_char)
                {
                case 'a':
                    state_ = state::fa;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::fa:  
                switch (next_char)
                {
                case 'l': 
                    state_ = state::fal;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::fal:  
                switch (next_char)
                {
                case 's':
                    state_ = state::fals;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::fals:  
                switch (next_char)
                {
                case 'e':
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::n:  
                switch (next_char)
                {
                case 'u':
                    state_ = state::nu;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::nu:  
                switch (next_char)
                {
                case 'l': 
                    state_ = state::nul;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::nul:  
                switch (next_char)
                {
                case 'l': 
                    if (top_ == start_top)
                    {
                        ++stack_[start_top].minimum_structure_capacity;
                    }
                    state_ = state::ok;
                    break;
                default:
                    done = true; // Error
                    break;
                }
                break;
            case state::slash: 
                switch (next_char)
                {
                case '*':
                    state_ = state::slash_star;
                    break;
                case '/':
                    state_ = state::slash_slash;
                    break;
                default:    
                    done = true; // Error
                    break;
                }
                break;
            case state::slash_star:  
                switch (next_char)
                {
                case '*':
                    state_ = state::slash_star_star;
                    break;
                }
                break;
            case state::slash_slash: 
                switch (next_char)
                {
                case '\n':
                case '\r':
                    state_ = saved_state_;
                    break;
                }
                break;
            case state::slash_star_star: 
                switch (next_char)
                {
                case '/':
                    state_ = saved_state_;
                    break;
                default:    
                    state_ = state::slash_star;
                    break;
                }
                break;
            }
            prev_char_ = next_char;
        }

        state_ = start_state;
        top_ = start_top;
        saved_state_ = start_saved_state;
        stack_[top_].mode = start_mode;
    }

    bool check_done()
    {
        bool result = (state_ == state::ok) && pop(mode::done);
        return result;
    }

    int state_;
    int depth_;
    int top_;
    std::vector<stack_item> stack_;
    basic_json_input_handler<Char> *handler_;
    basic_parse_error_handler<Char> *err_handler_;
    std::basic_istream<Char> *is_;
    unsigned long column_;
    unsigned long line_;
    Char c_;
    bool eof_;
    uint32_t cp_;
    uint32_t cp2_;
    std::vector<Char> buffer_;
    size_t buffer_length_;
    size_t buffer_capacity_;
    std::basic_string<Char> string_buffer_;
    bool is_negative_;
    int saved_state_;
    int prev_char_;
};

typedef basic_json_reader<char> json_reader;
typedef basic_json_reader<wchar_t> wjson_reader;

}

#endif
