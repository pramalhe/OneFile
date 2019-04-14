/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.

 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _CX_TREE_SET_H_
#define _CX_TREE_SET_H_

#include <iostream>
#include <functional>
#include <set>

//#include "../datastructures/sequential/RedBlackBST.hpp"

// TODO: change CKey* to CKey&

// This is a wrapper to std::set, which should be a Red-Black tree
template<typename CKey>
class TreeSet {

private:
    std::set<CKey> set;
    // Use this instead if we want to have control over the Red-Black tree
    //RedBlackBST<CKey,CKey> set;

public:

    static std::string className() { return "TreeSet"; }


    bool add(CKey key) {
        if (set.find(key) == set.end()) {
            set.insert(key); // TODO: can we improve this so we don't have to lookup twice?
            return true;
        }
        return false;
    }

    bool remove(CKey key) {
        auto iter = set.find(key);
        if (iter == set.end()) return false;
        set.erase(iter);
        return true;
    }

    bool contains(CKey key) {
        if (set.find(key) == set.end()) return false;
        return true;  // TODO: optimize this
    }

    bool iterateAll(std::function<bool(CKey*)> itfunc) {
		for (auto it = set.begin(); it != set.end(); ++it) {
			CKey key = *it;
			if (!itfunc(&key)) return false;
		}
		return true;
    }
/*
    bool add(CKey* key) {
        return set.add(key, 0);
    }

    bool remove(CKey* key) {
        return set.remove(key, 0);
    }

    bool contains(CKey* key) {
        return set.contains(key, 0);
    }
*/
};

#endif /* _TREE_SET_H_ */
