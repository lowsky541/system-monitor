#ifndef __IG_UTILITIES__
#define __IG_UTILITIES__

#include <map>
#include <functional>

template<typename A, typename B>
std::map<A, B> intersect_apply(
    const std::map<A, B>& first,
    const std::map<A, B>& second,
    std::function<B(B, B)> pred)
{
    std::map<A, B> result;
    for (const auto& second_it : second) {
        const auto& first_it = first.find(second_it.first);
        if (first_it != first.end()) {
            result[second_it.first] = pred(first_it->second, second_it.second);
        }
    }
    return result;
}

float average(const float *v, const int l)
{
    float sum = 0;
    for (int i = 0; i < l; i++)
        sum += v[i];
    return sum / l;
}

#endif