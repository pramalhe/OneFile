/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Adapted from https://github.com/roghnin/Interval-Based-Reclamation/blob/master/src/rideables/NatarajanTree.hpp

Due to the usage of <optional>, this needs C++17 to compile

Pedro: I've adapted this for our benchmarks but the adaptation may contain errors, please do not use this code in production!
*/


#ifndef _NATARAJAN_TREE_HAZARD_ERAS_H_
#define _NATARAJAN_TREE_HAZARD_ERAS_H_

#include <iostream>
#include <atomic>
#include <algorithm>
#include <map>
#include <optional>
#include "common/HazardEras.hpp"


template <class K, class V>
class NatarajanTreeHE {
private:
    const int MAX_THREADS = 128;
    /* structs*/
    struct Node {
        int level;
        K key;
        V val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        uint64_t newEra {0};          // TODO: put he.getEra() here
        uint64_t delEra;
        Node(uint64_t newEra) : newEra{newEra} {};
        Node(uint64_t newEra, K k, V v, Node* l, Node* r,int lev):level(lev),key(k),val(v),left(l),right(r),newEra{newEra} {};
        Node(uint64_t newEra, K k, V v, Node* l, Node* r):level(-1),key(k),val(v),left(l),right(r),newEra{newEra} {};
    };
    struct SeekRecord{
        Node* ancestor;
        Node* successor;
        Node* parent;
        Node* leaf;
    };

    /* variables */
    HazardEras<Node> he {5, MAX_THREADS};

    K infK{};
    V defltV{};
    Node* r;
    Node* s;
    SeekRecord* records;
    const size_t GET_POINTER_BITS = 0xfffffffffffffffc;//for machine 64-bit or less.

    /* helper functions */
    //flag and tags helpers
    inline Node* getPtr(Node* mptr){
        return (Node*) ((size_t)mptr & GET_POINTER_BITS);
    }
    inline bool getFlg(Node* mptr){
        return (bool)((size_t)mptr & 1);
    }
    inline bool getTg(Node* mptr){
        return (bool)((size_t)mptr & 2);
    }
    inline Node* mixPtrFlgTg(Node* ptr, bool flg, bool tg){
        return (Node*) ((size_t)ptr | flg | ((size_t)tg<<1));
    }
    //node comparison
    inline bool isInf(Node* n){
        return getInfLevel(n)!=-1;
    }
    inline int getInfLevel(Node* n){
        //0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
        n=getPtr(n);
        return n->level;
    }
    inline bool nodeLess(Node* n1, Node* n2){
        n1=getPtr(n1);
        n2=getPtr(n2);
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
    }
    inline bool nodeEqual(Node* n1, Node* n2){
        n1=getPtr(n1);
        n2=getPtr(n2);
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        if(i1==-1&&i2==-1)
            return n1->key==n2->key;
        else
            return i1==i2;
    }
    inline bool nodeLessEqual(Node* n1, Node* n2){
        return !nodeLess(n2,n1);
    }

    /* private interfaces */
    void seek(K key, int tid);
    bool cleanup(K key, int tid);
    void doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res);
public:
    NatarajanTreeHE(const int maxThreads=0) {
        r = new Node(he.getEra(), infK,defltV,nullptr,nullptr,2);
        s = new Node(he.getEra(), infK,defltV,nullptr,nullptr,1);
        r->right = new Node(he.getEra(), infK,defltV,nullptr,nullptr,2);
        r->left = s;
        s->right = new Node(he.getEra(), infK,defltV,nullptr,nullptr,1);
        s->left = new Node(he.getEra(), infK,defltV,nullptr,nullptr,0);
        records = new SeekRecord[MAX_THREADS]{};
    };
    ~NatarajanTreeHE(){};

    static std::string className() { return "NatarajanTreeHE"; }
    std::optional<V> get(K key, int tid);
    std::optional<V> put(K key, V val, int tid);
    bool insert(K key, V val, int tid);
    std::optional<V> innerRemove(K key, int tid);
    std::optional<V> replace(K key, V val, int tid);
    std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid);

    // Used only by our tree benchmarks
    bool add(K key, int tid);
    bool remove(K key, int tid);
    bool contains(K key, int tid);
    void addAll(K** keys, const int size, const int tid);
};

//-------Definition----------
template <class K, class V>
void NatarajanTreeHE<K,V>::seek(K key, int tid){
    /* initialize the seek record using sentinel nodes */
    Node keyNode{he.getEra(),key,defltV,nullptr,nullptr};//node to be compared
    SeekRecord* seekRecord = &(records[tid]);
    seekRecord->ancestor = r;
    seekRecord->successor = he.get_protected(1, r->left, tid);
    seekRecord->parent = he.get_protected(2, r->left, tid);
    seekRecord->leaf = getPtr(he.get_protected(3, s->left, tid));

    /* initialize other variables used in the traversal */
    Node* parentField = he.get_protected(3, seekRecord->parent->left, tid);
    Node* currentField = he.get_protected(4, seekRecord->leaf->left,tid);
    Node* current = getPtr(currentField);

    /* traverse the tree */
    while(current!=nullptr){
        /* check if the edge from the current parent node is tagged */
        if(!getTg(parentField)){
            /*
             * found an untagged edge in the access path;
             * advance ancestor and successor pointers.
             */
            seekRecord->ancestor=seekRecord->parent;
            he.protectEraRelease(0, 1, tid);
            seekRecord->successor=seekRecord->leaf;
            he.protectEraRelease(1, 3, tid);
        }

        /* advance parent and leaf pointers */
        seekRecord->parent = seekRecord->leaf;
        he.protectEraRelease(2, 3, tid);
        seekRecord->leaf = current;
        he.protectEraRelease(3, 4, tid);

        /* update other variables used in traversal */
        parentField=currentField;
        if(nodeLess(&keyNode,current)){
            currentField = he.get_protected(4, current->left, tid);
        }
        else{
            currentField = he.get_protected(4, current->right, tid);
        }
        current=getPtr(currentField);
    }
    /* traversal complete */
    return;
}

template <class K, class V>
bool NatarajanTreeHE<K,V>::cleanup(K key, int tid){
    Node keyNode{he.getEra(),key,defltV,nullptr,nullptr};//node to be compared
    bool res=false;

    /* retrieve addresses stored in seek record */
    SeekRecord* seekRecord=&(records[tid]);
    Node* ancestor=getPtr(seekRecord->ancestor);
    Node* successor=getPtr(seekRecord->successor);
    Node* parent=getPtr(seekRecord->parent);
    Node* leaf=getPtr(seekRecord->leaf);

    std::atomic<Node*>* successorAddr=nullptr;
    std::atomic<Node*>* childAddr=nullptr;
    std::atomic<Node*>* siblingAddr=nullptr;

    /* obtain address of field of ancestor node that will be modified */
    if(nodeLess(&keyNode,ancestor))
        successorAddr=&(ancestor->left);
    else
        successorAddr=&(ancestor->right);

    /* obtain addresses of child fields of parent node */
    if(nodeLess(&keyNode,parent)){
        childAddr=&(parent->left);
        siblingAddr=&(parent->right);
    }
    else{
        childAddr=&(parent->right);
        siblingAddr=&(parent->left);
    }
    Node* tmpChild=childAddr->load(std::memory_order_acquire);
    if(!getFlg(tmpChild)){
        /* the leaf is not flagged, thus sibling node should be flagged */
        tmpChild=siblingAddr->load(std::memory_order_acquire);
        /* switch the sibling address */
        siblingAddr=childAddr;
    }

    /* use TAS to tag sibling edge */
    while(true){
        Node* untagged=siblingAddr->load(std::memory_order_acquire);
        Node* tagged=mixPtrFlgTg(getPtr(untagged),getFlg(untagged),true);
        if(siblingAddr->compare_exchange_strong(untagged,tagged,std::memory_order_acq_rel)){
            break;
        }
    }
    /* read the flag and address fields */
    Node* tmpSibling=siblingAddr->load(std::memory_order_acquire);

    /* make the sibling node a direct child of the ancestor node */
    res=successorAddr->compare_exchange_strong(successor,
        mixPtrFlgTg(getPtr(tmpSibling),getFlg(tmpSibling),false),
        std::memory_order_acq_rel);

    if(res==true){
        he.retire(getPtr(tmpChild),tid);
        he.retire(successor,tid);
    }
    return res;
}

/* to test rangeQuery */
// template <>
// optional<int> NatarajanTree<int,int>::get(int key, int tid){
//  int len=0;
//  auto x = rangeQuery(key-500,key,len,tid);
//  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
//  optional<int> res={};
//  SeekRecord* seekRecord=&(records[tid].ui);
//  Node* leaf=nullptr;
//  seek(key,tid);
//  leaf=getPtr(seekRecord->leaf);
//  if(nodeEqual(&keyNode,leaf)){
//      res = leaf->val;
//  }
//  return res;
// }

template <class K, class V>
std::optional<V> NatarajanTreeHE<K,V>::get(K key, int tid){
    Node keyNode{he.getEra(),key,defltV,nullptr,nullptr};//node to be compared
    std::optional<V> res={};
    SeekRecord* seekRecord=&(records[tid]);
    Node* leaf=nullptr;
    seek(key,tid);
    leaf=getPtr(seekRecord->leaf);
    if(nodeEqual(&keyNode,leaf)){
        res = leaf->val;
    }
    he.clear(tid);
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeHE<K,V>::put(K key, V val, int tid){
    std::optional<V> res={};
    SeekRecord* seekRecord=&(records[tid]);

    Node* newInternal=nullptr;
    Node* newLeaf = new Node(he.getEra(),key,val,nullptr,nullptr);//also to compare keys

    Node* parent=nullptr;
    Node* leaf=nullptr;
    std::atomic<Node*>* childAddr=nullptr;

    while(true){
        seek(key,tid);
        leaf=getPtr(seekRecord->leaf);
        parent=getPtr(seekRecord->parent);
        if(!nodeEqual(newLeaf,leaf)){//key does not exist
            /* obtain address of the child field to be modified */
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);

            /* create left and right leave of newInternal */
            Node* newLeft=nullptr;
            Node* newRight=nullptr;
            if(nodeLess(newLeaf,leaf)){
                newLeft=newLeaf;
                newRight=leaf;
            }
            else{
                newLeft=leaf;
                newRight=newLeaf;
            }

            /* create newInternal */
            if(isInf(leaf)){
                int lev=getInfLevel(leaf);
                newInternal = new Node(he.getEra(),infK,defltV,newLeft,newRight,lev);
            }
            else
                newInternal = new Node(he.getEra(),std::max(key,leaf->key),defltV,newLeft,newRight);

            /* try to add the new nodes to the tree */
            Node* tmpExpected=getPtr(leaf);
            if(childAddr->compare_exchange_strong(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
                res={};
                break;//insertion succeeds
            }
            else{//fails; help conflicting delete operation
                delete newInternal;
                Node* tmpChild=childAddr->load(std::memory_order_acquire);
                if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                    /*
                     * address of the child has not changed
                     * and either the leaf node or its sibling
                     * has been flagged for deletion
                     */
                    cleanup(key,tid);
                }
            }
        }
        else{//key exists, update and return old
            res=leaf->val;
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);
            if(childAddr->compare_exchange_strong(leaf,newLeaf,std::memory_order_acq_rel)){
                he.retire(leaf,tid);
                break;
            }
        }
    }
    he.clear(tid);
    return res;
}

template <class K, class V>
bool NatarajanTreeHE<K,V>::insert(K key, V val, int tid) {
    bool res=false;
    SeekRecord* seekRecord=&(records[tid]);

    Node* newInternal=nullptr;
    Node* newLeaf = new Node(he.getEra(),key,val,nullptr,nullptr);//also for comparing keys

    Node* parent=nullptr;
    Node* leaf=nullptr;
    std::atomic<Node*>* childAddr=nullptr;
    while(true){
        seek(key,tid);
        leaf=getPtr(seekRecord->leaf);
        parent=getPtr(seekRecord->parent);
        if(!nodeEqual(newLeaf,leaf)){//key does not exist
            /* obtain address of the child field to be modified */
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);

            /* create left and right leave of newInternal */
            Node* newLeft=nullptr;
            Node* newRight=nullptr;
            if(nodeLess(newLeaf,leaf)){
                newLeft=newLeaf;
                newRight=leaf;
            }
            else{
                newLeft=leaf;
                newRight=newLeaf;
            }

            /* create newInternal */
            if(isInf(leaf)){
                int lev=getInfLevel(leaf);
                newInternal = new Node(he.getEra(),infK,defltV,newLeft,newRight,lev);
            }
            else
                newInternal = new Node(he.getEra(),std::max(key,leaf->key),defltV,newLeft,newRight);

            /* try to add the new nodes to the tree */
            Node* tmpExpected=getPtr(leaf);
            if(childAddr->compare_exchange_strong(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
                res=true;
                break;//insertion succeeds
            }
            else{//fails; help conflicting delete operation
                delete newInternal;
                Node* tmpChild=childAddr->load(std::memory_order_acquire);
                if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                    /*
                     * address of the child has not changed
                     * and either the leaf node or its sibling
                     * has been flagged for deletion
                     */
                    cleanup(key,tid);
                }
            }
        }
        else{//key exists, insertion fails
            delete newLeaf;
            res=false;
            break;
        }
    }
    he.clear(tid);
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeHE<K,V>::innerRemove(K key, int tid){
    bool injecting = true;
    std::optional<V> res={};
    SeekRecord* seekRecord=&(records[tid]);

    Node keyNode{he.getEra(),key,defltV,nullptr,nullptr};//node to be compared

    Node* parent=nullptr;
    Node* leaf=nullptr;
    std::atomic<Node*>* childAddr=nullptr;
    while(true){
        seek(key,tid);
        parent=getPtr(seekRecord->parent);
        /* obtain address of the child field to be modified */
        if(nodeLess(&keyNode,parent))
            childAddr=&(parent->left);
        else
            childAddr=&(parent->right);

        if(injecting){
            /* injection mode: check if the key exists */
            leaf=getPtr(seekRecord->leaf);
            if(!nodeEqual(leaf,&keyNode)){//does not exist
                res={};
                break;
            }

            /* inject the delete operation into the tree */
            Node* tmpExpected=getPtr(leaf);
            res=leaf->val;
            if(childAddr->compare_exchange_strong(tmpExpected,
                mixPtrFlgTg(tmpExpected,true,false), std::memory_order_acq_rel)){
                /* advance to cleanup mode to remove the leaf node */
                injecting=false;
                if(cleanup(key,tid)) break;
            }
            else{
                Node* tmpChild=childAddr->load(std::memory_order_acquire);
                if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                    /*
                     * address of the child has not
                     * changed and either the leaf
                     * node or its sibling has been
                     * flagged for deletion
                     */
                    cleanup(key,tid);
                }
            }
        }
        else{
            /* cleanup mode: check if flagged node still exists */
            if(seekRecord->leaf!=leaf){
                /* leaf no longer in the tree */
                break;
            }
            else{
                /* leaf still in the tree; remove */
                if(cleanup(key,tid)) break;
            }
        }
    }
    he.clear(tid);
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeHE<K,V>::replace(K key, V val, int tid){
    std::optional<V> res={};
    SeekRecord* seekRecord=&(records[tid]);

    Node* newInternal=nullptr;
    Node* newLeaf = new Node(he.getEra(),key,val,nullptr,nullptr);//also to compare keys

    Node* parent=nullptr;
    Node* leaf=nullptr;
    std::atomic<Node*>* childAddr=nullptr;
    while(true){
        seek(key,tid);
        parent=getPtr(seekRecord->parent);
        leaf=getPtr(seekRecord->leaf);
        if(!nodeEqual(newLeaf,leaf)){//key does not exist, replace fails
            delete newLeaf;
            res={};
            break;
        }
        else{//key exists, update and return old
            res=leaf->val;
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);
            if(childAddr->compare_exchange_strong(leaf,newLeaf,std::memory_order_acq_rel)){
                he.retire(leaf,tid);
                break;
            }
        }
    }
    he.clear(tid);
    return res;
}

template <class K, class V>
std::map<K, V> NatarajanTreeHE<K,V>::rangeQuery(K key1, K key2, int& len, int tid){
    //NOT HP-like GC safe.
    if(key1>key2) return {};
    Node k1{he.getEra(),key1,defltV,nullptr,nullptr};//node to be compared
    Node k2{he.getEra(),key2,defltV,nullptr,nullptr};//node to be compared

    Node* leaf = getPtr(he.get_protected(0, s->left, tid));
    Node* current = getPtr(he.get_protected(1, leaf->left, tid));

    std::map<K,V> res;
    if(current!=nullptr)
        doRangeQuery(k1,k2,tid,current,res);
    len=res.size();
    return res;
}

template <class K, class V>
void NatarajanTreeHE<K,V>::doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res){
    Node* left = getPtr(he.get_protected(2, root->left, tid));
    Node* right = getPtr(he.get_protected(3, root->right, tid));
    if(left==nullptr&&right==nullptr){
        if(nodeLessEqual(&k1,root)&&nodeLessEqual(root,&k2)){
            res.emplace(root->key,root->val);
        }
        return;
    }
    if(left!=nullptr){
        if(nodeLess(&k1,root)){
            doRangeQuery(k1,k2,tid,left,res);
        }
    }
    if(right!=nullptr){
        if(nodeLessEqual(root,&k2)){
            doRangeQuery(k1,k2,tid,right,res);
        }
    }
    return;
}


// Wrappers for the "set" benchmarks
template <class K, class V>
bool NatarajanTreeHE<K,V>::add(K key, int tid) {
    return insert(key,key,tid);
}

template <class K, class V>
bool NatarajanTreeHE<K,V>::remove(K key, int tid) {
    return innerRemove(key,tid).has_value();
}

template <class K, class V>
bool NatarajanTreeHE<K,V>::contains(K key, int tid) {
    return get(key,tid).has_value();
}

// Not lock-free
template <class K, class V>
void NatarajanTreeHE<K,V>::addAll(K** keys, const int size, const int tid) {
    for (int i = 0; i < size; i++) add(*keys[i], tid);
}

#endif
