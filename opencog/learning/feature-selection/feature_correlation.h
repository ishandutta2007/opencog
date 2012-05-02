/** feature_correlation.h ---
 *
 * Copyright (C) 2010,2012 OpenCog Foundation
 *
 * Authors: Nil Geisweiller <nilg@laptop>
 *          Linas Vepstas <linasvepstas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _OPENCOG_FEATURE_CORRELATION_ALGO_H
#define _OPENCOG_FEATURE_CORRELATION_ALGO_H

#include <functional>

#include <opencog/util/numeric.h>
#include <opencog/util/foreach.h>
#include <opencog/util/lru_cache.h>
#include <opencog/util/algorithm.h>
#include <opencog/util/functional.h>
#include <opencog/util/oc_omp.h>

namespace opencog {

/**
 * Returns a set S of features following the algo:
 * 0) set<FeatureSet> tops = empty set
 * 1) For each FeatureSet fs in tops:
 * 2)     Add one feature, compute new MI. Repeat 1)
 * 3) Discard all but top_size of highest scorers
 * 4) tops = the highest scorers.
 * 5) Repeat, until tops hold FeatureSets of 'num_features' features.
 * 6) return highest scorer from tops.
 *
 * @param features       The initial set of features to be selected from
 * @param scorer         The function to score a set of features.
 * @param num_features   The desired number of features to return.
 * @param top_size       Number of top-correlated feature sets to explore.
 *
 * @return               The set of selected features
 */
template<typename Scorer, typename FeatureSet>
FeatureSet correlation_selection(const FeatureSet& features,
                                 const Scorer& scorer,
                                 unsigned num_features,
                                 unsigned top_size=100)
{
    if (logger().isDebugEnabled()) {
        logger().debug() << "Call correlation_selection(num_features="
                         << num_features
                         << ", top_size=" << top_size
                         <<")";
    }


    typedef boost::shared_mutex shared_mutex;
    typedef boost::shared_lock<shared_mutex> shared_lock;
    typedef boost::unique_lock<shared_mutex> unique_lock;
    shared_mutex mutex;

    // Start with the empty set
    std::set<FeatureSet> tops;

    // Repeat, until we've gotton the highest-ranked FeatueSet
    // that has 'num_features' in it.
    for (unsigned i = 1; i <= num_features; ++i) {
        std::map<double, FeatureSet> ranks;

        // Add one feature at a time to fs, and score the
        // result.  Rank the result.
        auto rank_em = [&](const FeatureSet &fs)
        {
            typename FeatureSet::const_iterator fi;
            for (fi = features.begin(); fi != features.end(); fi++) {
                FeatureSet prod = fs;
                prod.insert(*fi);

                double mi = scorer(prod);

                unique_lock lock(mutex);
                /// @todo this lock can be more granular
                ranks.insert(std::pair<double, FeatureSet>(mi, prod));
            }
        };
        OMP_ALGO::for_each(tops.begin(), tops.end(), rank_em);

        // First time through, only.  Just rank one feature.
        if (tops.size() == 0) {
            FeatureSet emptySet;
            rank_em(emptySet);
        }

        // Discard all but the highest scorers.
        tops.clear();
        unsigned j = 0;
        typename std::map<double, FeatureSet>::const_reverse_iterator mit;
        for (mit = ranks.rbegin(); mit != ranks.rend(); mit++) {
#if DEBUG
            std::cout << "MI=" << mit->first << " feats=";
            typename FeatureSet::const_iterator fi;
            FeatureSet ff = mit->second;
            for (fi = ff.begin(); fi != ff.end(); fi++) {
                std::cout << "-" << *fi;
             }
             std::cout << std::endl;
#endif
            tops.insert(mit->second);
            j++;
            if (top_size < j) break;
        }
    }

    // If we did this correctly, then the highest scorer is at the
    // end of the set.  Return that.
    FeatureSet res;
    if (!tops.empty()) res = *tops.rbegin();

    if (logger().isDebugEnabled()) {
        std::stringstream ss;
        ss << "Exit correlation_selection(), selected: ";
        typename FeatureSet::const_iterator fi;
        for (fi = res.begin(); fi != res.end(); fi++) {
            ss << " " << *fi;
        }
        logger().debug() << ss.str();
    }
    return res;
}

} // ~namespace opencog

#endif // _OPENCOG_FEATURE_CORRELATION_ALGO_H