#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <iterator>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace std;

namespace Windscribe { 

/** Several algorithms on sets and segments. */
template<typename T>
class Algorithms
{
public:

    /** Returns intersection between 2 sets taking into account repetitions. O(n) but with allocation. */
    vector<T> intersection(const vector<T>& v1, const vector<T>& v2);

    /** Returns intersection between 2 sets taking into account repetitions. O(nlnn). */
    vector<T> intersection2(vector<T>& v1, vector<T>& v2);

private:
    /** Returns hash table where key is element and value is count of its repetition in vector range. O(n) */
    template<typename Counter>
    unordered_map<T, Counter> buildHash(typename vector<T>::const_iterator s, typename vector<T>::const_iterator e);

    /**
    * Finds repeated elements from vector range and hash table.
    * @todo Type checking for T to have overloaded operator == and < and for Counter to have ++, operator int() and --.
    */
    template<typename Counter>
    vector<T> intersection(typename vector<T>::const_iterator s1, typename vector<T>::const_iterator e1, unordered_map<T, Counter>& hashTable, Counter& elementsCount);
};

template<typename T>
inline vector<T> Algorithms<T>::intersection(const vector<T>& v1, const vector<T>& v2)
{
    if (v1.empty() || v2.empty())
        return {};

    // @note For enough small v1 and v2 simple O(n2) array traversal can be faster.
    // @todo For now do not do tests to see if such solution is more efficient for this case.

    const vector<T>* big = v1.size() >= v2.size() ? &v1 : &v2;
    const vector<T>* small = v1.size() < v2.size() ? &v1 : &v2;

    auto hashTable = buildHash<size_t>(small->cbegin(), small->cend());
    size_t count{ small->size() };
    return intersection<size_t>(big->cbegin(), big->cend(), hashTable, count);
}

template<typename T>
inline vector<T> Algorithms<T>::intersection2(vector<T>& v1, vector<T>& v2)
{
    if (v1.empty() || v2.empty())
        return {};

    sort(v1.begin(), v1.end());
    sort(v2.begin(), v2.end());

    int i{}, j{};
    vector<T> res;
    res.reserve(v1.size() / 10); // @todo find better allocation strategy.
    const auto size1 = v1.size();
    const auto size2 = v2.size();
    while(i < size1 && j < size2) {
        if (v1.at(i) == v2.at(j)) {
            res.push_back(v1.at(i));
            i++;
            j++;
        }
        else if (v1.at(i) < v2.at(j)) {
            i++;
        }
        else {
            j++;
        }
    }
    return res;
}

template<typename T>
template<typename Counter>
inline unordered_map<T, Counter> Algorithms<T>::buildHash(typename vector<T>::const_iterator s, typename vector<T>::const_iterator e)
{
    unordered_map<T, Counter> res;
    for (auto it = s; it < e; ++it) {
        if (res.find(*it) == res.cend())
            res.insert(make_pair(*it, 1));
        else
            res[*it]++;
    }
    return res;
}

template<typename T>
template<typename Counter>
inline vector<T> Algorithms<T>::intersection(typename vector<T>::const_iterator s1, typename vector<T>::const_iterator e1, unordered_map<T, Counter>& hashTable, Counter& elementsCount)
{
    vector<T> res;
    res.reserve(e1 - s1);
    for (auto it = s1; it < e1; ++it) {
        if (hashTable.find(*it) != hashTable.cend() && hashTable[*it])
        {
            res.push_back(*it);
            hashTable[*it]--;
            elementsCount--;
            if (!elementsCount)
                return res;
        }
    }

    return res;
}

}
