#ifndef KMCOMP_FAST_MEDIAN_H
#define KMCOMP_FAST_MEDIAN_H

#include <vector>
#include <algorithm>

namespace kmcomp
{
    //O(n.log(n)) exact: sort & take middle value
    double nlogn_median(const std::vector<double>& distances);

    //O(n) approximate: quickselect using median of medians
    double quickselect(std::vector<double>& distances, unsigned k);
    double quickselect_median(std::vector<double>& distances);
    double approximate_median(std::vector<double>& distances);
};
#endif