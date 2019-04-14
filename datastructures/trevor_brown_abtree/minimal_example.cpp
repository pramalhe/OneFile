/**
 * Author: Trevor Brown (me [at] tbrown [dot] pro).
 * Copyright 2018.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <iostream>
#include <limits>
#include <cassert>

#include "brown_ext_abtree_lf_adapter.h"

int main(int argc, char** argv) {

    const int NODE_DEGREE = 16;
    const int ANY_KEY = 0;
    const int NUM_THREADS = 1;
    
    auto tree = new ds_adapter<NODE_DEGREE, int>(NUM_THREADS, ANY_KEY);
    
    const int threadID = 0;

    tree->initThread(threadID);
    
    void * oldVal = tree->insertIfAbsent(threadID, 7, (void *) 1020);
    assert(oldVal == tree->getNoValue());
    
    bool result = tree->contains(threadID, 7);
    assert(result);
    
    result = tree->contains(threadID, 8);
    assert(!result);
    
    void * val = tree->find(threadID, 7);
    assert(val == (void *) 1020);
    
    val = tree->erase(threadID, 7);
    assert(val == (void *) 1020);
    
    result = tree->contains(threadID, 7);
    assert(!result);
    
    tree->deinitThread(threadID);
    
    delete tree;
    
    std::cout<<"Passed quick tests."<<std::endl;
            
    return 0;
}
