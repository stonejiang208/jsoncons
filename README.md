# jsoncons: a C++ library for json construction

jsoncons is a C++ library for the construction of [JavaScript Object Notation (JSON)](http://www.json.org). It supports parsing a JSON file or string into a `json` value, building a `json` value in C++ code, and serializing a `json` value to a file or string. It also provides an API for generating json read and write events in code, somewhat analogously to SAX processing in the XML world. 

jsoncons uses some features that are new to C++ 11, including [move semantics](http://thbecker.net/articles/rvalue_references/section_02.html) and the [AllocatorAwareContainer](http://en.cppreference.com/w/cpp/concept/AllocatorAwareContainer) concept. It has been tested with MS VC++ 2013, MS VC++ 2015, GCC 4.8, GCC 4.9, and recent versions of clang. Note that `std::regex` isn't fully implemented in GCC 4.8., so `jsoncons_ext/jsonpath` regular expression filters aren't supported for that compiler.

The [code repository](https://github.com/danielaparker/jsoncons) and [releases](https://github.com/danielaparker/jsoncons/releases) are on github. It is distributed under the [Boost Software License](http://www.boost.org/users/license.html)

The library has a number of features, which are listed below:

- Uses the standard C++ input/output streams library
- Supports converting to and from the standard library sequence and associative containers
- Supports object members sorted alphabetically by name or in original order
- Implements parsing and serializing JSON text in UTF-8 for narrow character strings and streams
- Supports UTF16 (UTF32) encodings with size 2 (size 4) wide characters
- Correctly handles surrogate pairs in \uXXXX escape sequences
- Supports event based JSON parsing and serializing with user defined input and output handlers
- Accepts and ignores single line comments that start with //, and multi line comments that start with /* and end with */
- Supports optional escaping of the solidus (/) character
- Supports Nan, Inf and -Inf replacement
- Supports reading a sequence of JSON texts from a stream
- Supports optional escaping of non-ascii UTF-8 octets
- Allows extensions to the types accepted by the json class constructors, accessors and modifiers
- Supports reading (writing) JSON values from (to) CSV files
- Supports serializing JSON values to [JSONx](http://www.ibm.com/support/knowledgecenter/SS9H2Y_7.5.0/com.ibm.dp.doc/json_jsonx.html) (XML)
- Passes all tests from [JSON_checker](http://www.json.org/JSON_checker/) except `fail1.json`, which is allowed in [RFC7159](http://www.ietf.org/rfc/rfc7159.txt)
- Handles JSON texts of arbitrarily large depth of nesting, a limit can be set if desired
- Supports [Stefan Goessner's JsonPath](http://goessner.net/articles/JsonPath/)

As the `jsoncons` library has evolved, names have sometimes changed. To ease transition, jsoncons deprecates the old names but continues to support many of them. See the [deprecated list](https://github.com/danielaparker/jsoncons/wiki/deprecated) for the status of old names. The deprecated names can be suppressed by defining macro `JSONCONS_NO_DEPRECATED`, which is recommended for new code.

## Note
The `json` initializer-list constructor has been removed, it gives inconsistent results when an initializer has zero elements, or one element of the type being initialized (`json`). Please replace

`json j = {1,2,3}` with `json j = json::array{1,2,3}`, and 

`json j = {{1,2,3},{4,5,6}}` with `json j = json::array{json::array{1,2,3},json::array{4,5,6}}`

Initializer-list constructors are now supported in `json::object` as well as `json::array`, e.g.
```c++
json j = json::object{{"first",1},{"second",json::array{1,2,3}}};
```

## Benchmarks

[json_benchmarks](https://github.com/danielaparker/json_benchmarks) provides some measurements about how `jsoncons` compares to other `json` libraries.

## Using the jsoncons library

The jsoncons library is header-only: it consists solely of header files containing templates and inline functions, and requires no separately-compiled library binaries when linking. It has no dependence on other libraries. The accompanying test suite uses boost, but not the library itself.

To install the jsoncons library, download the zip file, unpack the release, under `src` find the directory `jsoncons`, and copy it to your `include` directory. If you wish to use extensions, copy the `jsoncons_ext` directory as well. 

For a quick guide, see the article [jsoncons: a C++ library for json construction](http://danielaparker.github.io/jsoncons). Consult the wiki for the latest [documentation and tutorials](https://github.com/danielaparker/jsoncons/wiki) and [roadmap](https://github.com/danielaparker/jsoncons/wiki/Roadmap). 

## Building the test suite and examples with CMake

[CMake](https://cmake.org/) is a C++ Makefiles/Solution generator for cross-platform software development. 

Instructions for building the test suite with CMake may be found in

    jsoncons/test_suite/build/cmake/README.txt

Instructions for building the examples with CMake may be found in

    jsoncons/examples/build/cmake/README.txt

## About jsoncons::basic_json

The jsoncons library provides a `basic_json` class template, which is the generalization of a `json` value for different character types, different policies for ordering name-value pairs, etc.
```c++
typedef basic_json<char,
                   JsonTraits = json_traits<char>,
                   Allocator = std::allocator<char>> json;
```
The library includes four instantiations of `basic_json`:

- [json](https://github.com/danielaparker/jsoncons/wiki/json) constructs a narrow character json value that sorts name-value members alphabetically

- [ojson](https://github.com/danielaparker/jsoncons/wiki/ojson) constructs a narrow character json value that retains the original name-value insertion order

- [wjson](https://github.com/danielaparker/jsoncons/wiki/wjson) constructs a wide character json value that sorts name-value members alphabetically

- [wojson](https://github.com/danielaparker/jsoncons/wiki/wojson) constructs a wide character json value that retains the original name-value insertion order

## Examples

The examples below illustrate the use of the [json](https://github.com/danielaparker/jsoncons/wiki/json) class and [json_query](https://github.com/danielaparker/jsoncons/wiki/json_query) function.

### json construction

```c++
#include "jsoncons/json.hpp"

// For convenience
using jsoncons::json;

// Construct a book object
json book1;

book1["category"] = "Fiction";
book1["title"] = "A Wild Sheep Chase: A Novel";
book1["author"] = "Haruki Murakami";
book1["date"] = "2002-04-09";
book1["price"] = 9.01;
book1["isbn"] = "037571894X";  

// Construct another using the set function
json book2;

book2.set("category", "History");
book2.set("title", "Charlie Wilson's War");
book2.set("author", "George Crile");
book2.set("date", "2007-11-06");
book2.set("price", 10.50);
book2.set("isbn", "0802143415");  

// Use set again, but more efficiently
json book3;

// Reserve memory, to avoid reallocations
book3.reserve(6);

// Insert in name alphabetical order
// Give set a hint where to insert the next member
auto hint = book3.set(book3.members().begin(),"author", "Haruki Murakami");
hint = book3.set(hint, "category", "Fiction");
hint = book3.set(hint, "date", "2006-01-03");
hint = book3.set(hint, "isbn", "1400079276");  
hint = book3.set(hint, "price", 13.45);
hint = book3.set(hint, "title", "Kafka on the Shore");

// Construct a fourth from a string
json book4 = json::parse(R"(
{
    "category" : "Fiction",
    "title" : "Pulp",
    "author" : "Charles Bukowski",
    "date" : "2004-07-08",
    "price" : 22.48,
    "isbn" : "1852272007"  
}
)");

// Construct a booklist array

json booklist = json::array();

// For efficiency, reserve memory, to avoid reallocations
booklist.reserve(4);

// For efficency, tell jsoncons to move the contents 
// of the four book objects into the array
booklist.add(std::move(book1));    
booklist.add(std::move(book2));    

// Add the third book to the front
auto pos = booklist.add(booklist.elements().begin(),std::move(book3));

// and the last one immediately after
booklist.add(pos+1,std::move(book4));    

// See what's left of book1, 2, 3 and 4 (expect nulls)
std::cout << book1 << "," << book2 << "," << book3 << "," << book4 << std::endl;


++
//Loop through the booklist elements using a range-based for loop    
for (const auto& book : booklist.elements())
{
    std::cout << book["title"].as<std::string>()
              << ","
              << book["price"].as<double>() << std::endl;
}

// The second book
json& book = booklist[1];

//Loop through the book's name-value pairs using a range-based for loop    
for (const auto& member : book.members())
{
    std::cout << member.name()
              << ","
              << member.value() << std::endl;
}

auto it = book.find("author");
if (it != book.members().end())
{
    // member "author" found
}

if (book.count("author") > 0)
{
    // book has a member "author"
}

book.get("author", "author unknown").as<std::string>();
// Returns author if found, otherwise "author unknown"

try
{
    book["ratings"].as<std::string>();
}
catch (const std::out_of_range& e)
{
    // member "ratings" not found
}

// Add ratings
book["ratings"]["*****"] = 4;
book["ratings"]["*"] = 2;

// Delete one-star ratings
book["ratings"].erase("*");

```
```c++  
    // Serialize the booklist to a file
    std::ofstream os("booklist.json");
    os << pretty_print(booklist);
```

The JSON output `booklist.json`
```json
[
    {
        "author":"Haruki Murakami",
        "category":"Fiction",
        "date":"2006-01-03",
        "isbn":"1400079276",
        "price":13.45,
        "title":"Kafka on the Shore"
    },
    {
        "author":"Charles Bukowski",
        "category":"Fiction",
        "date":"2004-07-08",
        "isbn":"1852272007",
        "price":22.48,
        "ratings":
        {
            "*****":4
        },
        "title":"Pulp"
    },
    {
        "author":"Haruki Murakami",
        "category":"Fiction",
        "date":"2002-04-09",
        "isbn":"037571894X",
        "price":9.01,
        "title":"A Wild Sheep Chase: A Novel"
    },
    {
        "author":"George Crile",
        "category":"History",
        "date":"2007-11-06",
        "isbn":"0802143415",
        "price":10.5,
        "title":"Charlie Wilson's War"
    }
]
```
### json query

```c++
#include <fstream>
#include "jsoncons/json.hpp"
#include "jsoncons_ext/jsonpath/json_query.hpp"

// For convenience
using jsoncons::json;
using jsoncons::jsonpath::json_query;

// Deserialize the booklist
std::ifstream is("booklist.json");
json booklist;
is >> booklist;

// Use a JsonPath expression to find 
//  
// (1) The authors of books that cost less than $12
json result = json_query(booklist, "$[*][?(@.price < 12)].author");
std::cout << result << std::endl;

// (2) The number of books
result = json_query(booklist, "$.length");
std::cout << result << std::endl;

// (3) The third book
result = json_query(booklist, "$[2]");
std::cout << std::endl << pretty_print(result) << std::endl;

// (4) The authors of books that were published in 2004
result = json_query(booklist, "$[*][?(@.date =~ /2004.*?/)].author");
std::cout << result << std::endl;

// (5) The titles of all books that have ratings
result = json_query(booklist, "$[*][?(@.ratings)].title");
std::cout << result << std::endl;

// (6) All authors and titles of books
result = json_query(booklist, "$..['author','title']");
std::cout << pretty_print(result) << std::endl;
```
Result:
```json
(1) ["Haruki Murakami","George Crile"]
(2) [4]
(3)
[
    {
        "author":"Haruki Murakami",
        "category":"Fiction",
        "date":"2002-04-09",
        "isbn":"037571894X",
        "price":9.01,
        "title":"A Wild Sheep Chase: A Novel"
    }
]
(4) ["Charles Bukowski"]
(5) ["Pulp"]
(6) 
[
    "Nigel Rees",
    "Sayings of the Century",
    "Evelyn Waugh",
    "Sword of Honour",
    "Herman Melville",
    "Moby Dick",
    "J. R. R. Tolkien",
    "The Lord of the Rings"
]
```
## Once again, this time with wide characters

### wjson construction

```c++
#include "jsoncons/json.hpp"

// For convenience
using jsoncons::wjson;

// Construct a book object
wjson book1;

book1[L"category"] = L"Fiction";
book1[L"title"] = L"A Wild Sheep Chase: A Novel";
book1[L"author"] = L"Haruki Murakami";
book1[L"date"] = L"2002-04-09";
book1[L"price"] = 9.01;
book1[L"isbn"] = L"037571894X";

// Construct another using the set function
wjson book2;

book2.set(L"category", L"History");
book2.set(L"title", L"Charlie Wilson's War");
book2.set(L"author", L"George Crile");
book2.set(L"date", L"2007-11-06");
book2.set(L"price", 10.50);
book2.set(L"isbn", L"0802143415");

// Use set again, but more efficiently
wjson book3;

// Reserve memory, to avoid reallocations
book3.reserve(6);

// Insert in name alphabetical order
// Give set a hint where to insert the next member
auto hint = book3.set(book3.members().begin(), L"author", L"Haruki Murakami");
hint = book3.set(hint, L"category", L"Fiction");
hint = book3.set(hint, L"date", L"2006-01-03");
hint = book3.set(hint, L"isbn", L"1400079276");
hint = book3.set(hint, L"price", 13.45);
hint = book3.set(hint, L"title", L"Kafka on the Shore");

// Construct a fourth from a string
wjson book4 = wjson::parse(LR"(
{
    "category" : "Fiction",
    "title" : "Pulp",
    "author" : "Charles Bukowski",
    "date" : "2004-07-08",
    "price" : 22.48,
    "isbn" : "1852272007"  
}
)");

// Construct a booklist array

wjson booklist = wjson::array();

// For efficiency, reserve memory, to avoid reallocations
booklist.reserve(4);

// For efficency, tell jsoncons to move the contents 
// of the four book objects into the array
booklist.add(std::move(book1));
booklist.add(std::move(book2));

// Add the third book to the front
auto pos = booklist.add(booklist.elements().begin(),std::move(book3));

// and the last one immediately after
booklist.add(pos+1,std::move(book4));    

// See what's left of book1, 2, 3 and 4 (expect nulls)
std::wcout << book1 << L"," << book2 << L"," << book3 << L"," << book4 << std::endl;

++
//Loop through the booklist elements using a range-based for loop    
for (const auto& book : booklist.elements())
{
    std::wcout << book[L"title"].as<std::wstring>()
               << L","
               << book[L"price"].as<double>() << std::endl;
}

// The second book
wjson& book = booklist[1];

//Loop through the book's name-value pairs using a range-based for loop    
for (const auto& member : book.members())
{
    std::wcout << member.name()
               << L","
               << member.value() << std::endl;
}

auto it = book.find(L"author");
if (it != book.members().end())
{
    // member "author" found
}

if (book.count(L"author") > 0)
{
    // book has a member "author"
}

book.get(L"author", L"author unknown").as<std::wstring>();
// Returns author if found, otherwise "author unknown"

try
{
    book[L"ratings"].as<std::wstring>();
}
catch (const std::out_of_range& e)
{
    // member "ratings" not found
}

// Add ratings
book[L"ratings"][L"*****"] = 4;
book[L"ratings"][L"*"] = 2;

// Delete one-star ratings
book[L"ratings"].erase(L"*");

```
```c++
// Serialize the booklist to a file
std::wofstream os("booklist2.json");
os << pretty_print(booklist);
```
### wjson query

```c++
// Deserialize the booklist
std::wifstream is("booklist2.json");
wjson booklist;
is >> booklist;

// Use a JsonPath expression to find 
//  
// (1) The authors of books that cost less than $12
wjson result = json_query(booklist, L"$[*][?(@.price < 12)].author");
std::wcout << result << std::endl;

// (2) The number of books
result = json_query(booklist, L"$.length");
std::wcout << result << std::endl;

// (3) The third book
result = json_query(booklist, L"$[2]");
std::wcout << pretty_print(result) << std::endl;

// (4) The authors of books that were published in 2004
result = json_query(booklist, L"$[*][?(@.date =~ /2004.*?/)].author");
std::wcout << result << std::endl;

// (5) The titles of all books that have ratings
result = json_query(booklist, L"$[*][?(@.ratings)].title");
std::wcout << result << std::endl;

// (6) All authors and titles of books
result = json_query(booklist, L"$..['author','title']");
std::wcout << pretty_print(result) << std::endl;
```
Result:
```json
(1) ["Haruki Murakami","George Crile"]
(2) [4]
(3)
[
    {
        "author":"Haruki Murakami",
        "category":"Fiction",
        "date":"2002-04-09",
        "isbn":"037571894X",
        "price":9.01,
        "title":"A Wild Sheep Chase: A Novel"
    }
]
(4) ["Charles Bukowski"]
(5) ["Pulp"]
(6) 
[
    "Nigel Rees",
    "Sayings of the Century",
    "Evelyn Waugh",
    "Sword of Honour",
    "Herman Melville",
    "Moby Dick",
    "J. R. R. Tolkien",
    "The Lord of the Rings"
]
```
## Acknowledgements

Special thanks to our [contributors](https://github.com/danielaparker/jsoncons/blob/master/acknowledgements.txt)

