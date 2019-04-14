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

#ifndef _SORTEDARRAYSET_H_
#define _SORTEDARRAYSET_H_

#include <iostream>

// TODO: Test this for correctness

/**
 * This is storing the pointers to the T instances, not the actual T instances.
 */
template<typename T>
class SortedArraySet {

private:
    unsigned max_size = 32;
    T** vec;  // TODO: change this to T if we change the API from T* to T&
    unsigned size = 0;
    static const int NOT_FOUND = 0;
    //std::atomic<bool> flag {false}; // For de debugging

    int lookup(T* key) {
        // Cover the special case of an empty array
        if (size== 0) return NOT_FOUND;
        int minPos = 0;
        int maxPos = size-1;
        //std::cout << "vec[0] = " << vec[0] << "\n";
        // Special cases for first and last items
        if (*key < *(vec[0])) return NOT_FOUND;
        if (*key == *(vec[0])) return 0;
        if (*key == *(vec[maxPos])) return maxPos;
        if (*(vec[maxPos]) < *key) return maxPos+1;
        while (true) {
            int pos = (maxPos-minPos)/2 + minPos;

            if (*key < *(vec[pos])) {
                maxPos = pos;
            } else if (*key == *(vec[pos])) {
                return pos;
            } else {
                minPos = pos;
            }
            if (maxPos-minPos <= 1) {
                return maxPos;
            }
        }
    }


public:
    SortedArraySet() {
        vec = new T*[max_size];
    }

    ~SortedArraySet() {
        delete[] vec;
    }

    // We need a copy constructor to be able to use it in CXMutation
    SortedArraySet(const SortedArraySet<T>& fromssv) {

        vec = new T*[fromssv.max_size];
        max_size = fromssv.max_size;
        size = fromssv.size;
        for(unsigned i=0;i<size;i++){
            vec[i]=fromssv.vec[i];
        }
    }

    static std::string className() { return "SortedArraySet"; }

    void erase(int index){
        //print();
        for(unsigned i=index;i<size-1;i++){
            vec[i] = vec[i+1];
        }
        size--;
        //print();
    }

    /**
     * When the curr.key is seen to be null it means we reached the tail node
     */
    bool remove(T* key) {
        //if (flag.load()) std::cout << "remove() ERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRROOOOOOOOOOOOOOR\n";
        //flag.store(true);
        unsigned index = lookup(key);
        if (index == size) {
            //std::cout<<"remove key "<<key->seq<<" "<<key->tid<<" vex "<<index<<"\n";
            //flag.store(false);
            return false;
        }
        if (*key == *(vec[index])) {
            erase(index);
            //flag.store(false);
            return true;
        }
        //flag.store(false);
        return false;
    }


    bool add(T* key) {
        //if (flag.load()) std::cout << "add() ERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRROOOOOOOOOOOOOOR\n";
        //flag.store(true);
        unsigned index = lookup(key);
        if (index != size && *key == *(vec[index])) {
            //std::cout<<"key "<<key->seq<<" "<<key->tid<<" vex "<<index<<" "<<vec[index]->seq<< " " << vec[index]->tid<<"\n";
            //assert(false);
            //flag.store(false);
            return false;
        }

        if(size+1==max_size){
            T** newvec = new T*[2*max_size];
            for(unsigned i=0;i<index;i++){
                newvec[i]=vec[i];
            }

            for(unsigned i=index;i<size;i++){
                newvec[i+1]=vec[i];
            }
            newvec[index]= key;
            delete[] vec;
            vec = newvec;
            max_size = 2*max_size;
        }else{
            if(index<size){
                for(unsigned i=size;i>=index+1;i--){
                    vec[i]=vec[i-1];
                }
            }
            vec[index] = key;
        }
        size++;
        //flag.store(false);
        return true;
    }


     bool contains(T* key) {
        //if (flag.load()) std::cout << "contains() ERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRROOOOOOOOOOOOOOR\n";
        unsigned index = lookup(key);
        if (index == size) {
            return false;
        }
        return *key == *(vec[index]);
    }


    bool print() { // For debug purposes
        for(unsigned i=0;i<size;i++){
            std:: cout << vec[i] << ",";
        }
        std::cout << "\n";
        return true;
    }

};

#endif /* _SORTEDARRAYSET_H_ */
