#include <memory>
#include <limits>
#include <cmath>
#include <iostream>
#include "../../TestFrame/FlyTest.hpp"
#include "../TypeConverter.hpp"

using namespace FuncLib;
using namespace Collections;
using namespace std;

TESTCASE("Type converter test")
{
    // TODO add TypeConvert::
    auto file = make_shared<File>("data");
    auto intLessThan = [](int const& a, int const& b) { return a < b; };
    auto strLessThan = [](string const& a, string const& b) { return a < b; };
    auto intPredPtr = make_shared<LessThan<int>>(intLessThan);
    auto strPredPtr = make_shared<LessThan<string>>(strLessThan);

    // c_x means converted_x

    SECTION("Basic type Convert")
    {
        auto i = 3;
        auto c_i = TypeConverter<int>::ConvertFrom(i, file);
        ASSERT(i == c_i);


        auto f = 1.23;// double
        auto c_f = TypeConverter<double>::ConvertFrom(f, file);
        ASSERT(f == c_f);

        auto c = 'a';
        auto c_c = TypeConverter<char>::ConvertFrom(c, file);
        ASSERT(c == c_c);

        string s = "Hello World";
        // cout << "Hello 1 time" << endl;
        // auto c_s = TypeConverter<string>::ConvertFrom(s, file);
        // ASSERT(s.size() == c_s->size());
    }

    SECTION("POD struct Convert")
    {
        struct A
        {
            int i;
            float f;
        };

        A a{ 1, 1.23 };
        auto c_a = TypeConverter<A>::ConvertFrom(a, file);
        ASSERT(c_a.i == a.i);
        ASSERT(c_a.f == a.f);
    }

    SECTION("NON-POD struct Convert")
    {
        // TODO
        struct A
        {
            int i;
            float f;
            string s;
        };
        A a{ 1, 1.23, "Hello World" };

        // auto c_a = TypeConverter<A>::ConvertFrom(a, file);
        // ASSERT(c_a.i == a.i);
        // ASSERT(c_a.f == a.f);
    }

    SECTION("LiteVector Convert")
    {
        // Not have this function
	}

    SECTION("Elements Convert")
    {
        Elements<int, int, 4> es(intPredPtr);
        es.Add({ 1, 1 });
        es.Add({ 2, 2 });
        auto c_es = TypeConverter<decltype(es)>::ConvertFrom(es, file);
        ASSERT(c_es.Count() == 2);
        ASSERT(c_es[0].first = 1);
        ASSERT(c_es[0].second = 1);
        ASSERT(c_es[1].second = 2);
        ASSERT(c_es[1].second = 2);

        // Elements<string, int, 4> es(predPtr); TODO
    }

    SECTION("Int-int LeafNode Convert")
    {
        using Leaf = LeafNode<int, int, 4>;
        Leaf l(intPredPtr);
        // auto c_l = TypeConverter<decltype(l)>::ConvertFrom(l, file);
    }

    SECTION("Int-int MiddleNode Convert")
    {
        using Middle = MiddleNode<int, int, 4>;
        Middle m(intPredPtr);
        // auto c_l = TypeConverter<decltype(m)>::ConvertFrom(m, file);
    }

    SECTION("Int-int Btree Convert")
    {
        using Tree = Btree<4, int, int>;
        Tree t(intLessThan);
        // auto c_t = TypeConverter<decltype(t)>::ConvertFrom(t, file);
    }

    // compare function and elements type need change
    // SECTION("String-int LeafNode Convert")
    // {
    //     using Leaf = LeafNode<string, int, 4>;
    //     Leaf l(strPredPtr);
    //     auto c_l = TypeConverter<decltype(l)>::ConvertFrom(l, file);
    // }

    // SECTION("String-int MiddleNode Convert")
    // {
    //     using Middle = MiddleNode<string, int, 4>;
    //     Middle m(strPredPtr);
    //     auto c_l = TypeConverter<decltype(m)>::ConvertFrom(m, file);
    // }

    // SECTION("String-int Btree Convert")
    // {
    //     using Tree = Btree<4, string, int>;
    //     Tree t(strLessThan);
    //     auto c_t = TypeConverter<decltype(t)>::ConvertFrom(t, file);
    // }
}

DEF_TEST_FUNC(typeConverterTest)