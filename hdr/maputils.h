#pragma once

#include <cstddef>


using namespace std;

/*
 * hash_combine and hash_range taken from boost
 *  Copyright 2005-2014 Daniel James.
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
template <class T>
inline void hash_combine(std::size_t& seed, T const& v) {
    seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

template <class It>
inline std::size_t hash_range(It first, It last) {
    std::size_t seed = 0;

    for(; first != last; ++first)
    {
        hash_combine(seed, *first);
    }

    return seed;
}



/*
 * hash used for bitcoin addresses. must be 25 bytes long!
 * hash value is simply bytes 12-20 of address. (assumption: addresses random)
 */
template <typename Container>
struct addr_hash {
    std::size_t operator()(Container const& c) const {
    	size_t hash = *(size_t*)&c[12];
    	return hash;
    }
};

/*
 * hash used for TX/Block hashes. must be 32 bytes long!
 * hash value is simply bytes 12-20 of hash. (assumption: hashes random)
 */
template <typename Container>
struct hash_hash {
    std::size_t operator()(Container const& c) const {
    	size_t hash = *(size_t*)&c[12];
    	return hash;
    }
};

/*
 * hash used for vectors
 */
template <typename Container>
struct container_hash {
    std::size_t operator()(Container const& c) const {
    	return hash_range(c.begin(), c.end());
    }

};

template <typename ContainerPair>
struct containerpair_hash {
    std::size_t operator()(ContainerPair const& c) const {
        return hash_range(c.first.begin(), c.first.end()) ^ hash_range(c.second.begin(), c.second.end()) ;
    }
};

template <typename ContainerPair>
struct containerpair_equal_to {
	bool operator() (const ContainerPair& x, const ContainerPair& y) const {
		return x.first == y.first && x.second == y.second;
    }
};
