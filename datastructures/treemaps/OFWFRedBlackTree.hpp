#ifndef _OF_WF_RED_BLACK_BST_H_
#define _OF_WF_RED_BLACK_BST_H_

#include <cassert>
#include <stdexcept>
#include <algorithm>

#include "stms/OneFileWF.hpp"               // This header defines the macros for the STM being compiled

// Adapted from Java to C++ from the original at http://algs4.cs.princeton.edu/code/edu/princeton/cs/algs4/RedBlackBST.java
template<typename K, typename V>
class OFWFRedBlackTree {
    const int64_t COLOR_RED   = 0;
    const int64_t COLOR_BLACK = 1;

    struct Node : public ofwf::tmbase {
        ofwf::tmtype<K>       key;
        ofwf::tmtype<V>       val;
        ofwf::tmtype<Node*>   left {nullptr};
        ofwf::tmtype<Node*>   right {nullptr};
        ofwf::tmtype<int64_t> color;    // color of parent link
        ofwf::tmtype<int64_t> size;     // subtree count
        Node(const K& key, const V& val, int64_t color, int64_t size) : key{key}, val{val}, color{color}, size{size} {}
    };

    ofwf::tmtype<Node*> root {nullptr};   // root of the BST

    inline void assignAndFreeIfNull(ofwf::tmtype<Node*>& z, Node* w) {
        Node* tofree = z;
        z = w;
        if (w == nullptr) ofwf::tmDelete(tofree);
    }

public:
    /**
     * Initializes an empty symbol table.
     */
    OFWFRedBlackTree(int numThreads=0){ }

    ~OFWFRedBlackTree() {
        for (int i = 0; i < 10000; i++) {
            ofwf::updateTx([&] () {
                if (root == nullptr) return;
                deleteMin();
            });
        }
    }

    /***************************************************************************
     *  Node helper methods.
     ***************************************************************************/
    // is node x red; false if x is null ?
    bool isRed(Node* x) {
        if (x == nullptr) return false;
        return x->color == COLOR_RED;
    }

    // number of node in subtree rooted at x; 0 if x is null
    int size(Node* x) {
        if (x == nullptr) return 0;
        return x->size;
    }


    /**
     * Returns the number of key-value pairs in this symbol table.
     * @return the number of key-value pairs in this symbol table
     */
    int size() {
        return size(root);
    }

    /**
     * Is this symbol table empty?
     * @return {@code true} if this symbol table is empty and {@code false} otherwise
     */
    bool isEmpty() {
        return root == nullptr;
    }


    /***************************************************************************
     *  Standard BST search->
     ***************************************************************************/

    /**
     * Returns the value associated with the given key.
     * @param key the key
     * @return the value associated with the given key if the key is in the symbol table
     *     and {@code null} if the key is not in the symbol table
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    bool innerGet(K key, V& oldValue, const bool saveOldValue) {
        bool found = get(root, key);
        if (!found) return false;
        //if (saveOldValue) oldValue = *val; // Copy of V
        return true;
    }

    // value associated with the given key in subtree rooted at x; null if no such key
    bool get(Node* x, K& key) {
        while (x != nullptr) {
            if      (key < x->key) x = x->left;
            else if (x->key < key) x = x->right;
            else              return true;
        }
        return false;
    }

    /**
     * Does this symbol table contain the given key?
     * @param key the key
     * @return {@code true} if this symbol table contains {@code key} and
     *     {@code false} otherwise
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    bool containsKey(const K& key) {
        return get(key) != nullptr;
    }

    /***************************************************************************
     *  Red-black tree insertion.
     ***************************************************************************/

    /**
     * Inserts the specified key-value pair into the symbol table, overwriting the old
     * value with the new value if the symbol table already contains the specified key.
     * Deletes the specified key (and its associated value) from this symbol table
     * if the specified value is {@code null}.
     *
     * @param key the key
     * @param val the value
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    bool innerPut(const K& key, const V& value) {
    	bool ret = false;
        root = put(root, key, value, ret);
        root->color = COLOR_BLACK;
        return ret;
    }

    // insert the key-value pair in the subtree rooted at h
    Node* put(Node* h, const K& key, const V& val, bool& ret) {
        if (h == nullptr) {
            ret = true;
            return ofwf::tmNew<Node>(key, val, COLOR_RED, 1);
        }
        if      (key < h->key) h->left  = put(h->left,  key, val, ret);
        else if (h->key < key) h->right = put(h->right, key, val, ret);
        else              h->val   = val;
        // fix-up any right-leaning links
        if (isRed(h->right) && !isRed(h->left))       h = rotateLeft(h);
        if (isRed(h->left)  &&  isRed(h->left->left)) h = rotateRight(h);
        if (isRed(h->left)  &&  isRed(h->right))      flipColors(h);
        h->size = size(h->left) + size(h->right) + 1;

        return h;
    }

    /***************************************************************************
     *  Red-black tree deletion.
     ***************************************************************************/

    /**
     * Removes the smallest key and associated value from the symbol table.
     * @throws NoSuchElementException if the symbol table is empty
     */
    void deleteMin() {
        if (isEmpty()) return;
        // if both children of root are black, set root to red
        if (!isRed(root->left) && !isRed(root->right))
            root->color = COLOR_RED;
        assignAndFreeIfNull(root, deleteMin(root));
        if (!isEmpty()) root->color = COLOR_BLACK;
        // assert check();
    }

    // delete the key-value pair with the minimum key rooted at h
    Node* deleteMin(Node* h) {
        if (h->left == nullptr)
            return nullptr;
        if (!isRed(h->left) && !isRed(h->left->left))
            h = moveRedLeft(h);
        assignAndFreeIfNull(h->left, deleteMin(h->left));
        return balance(h);
    }


    /**
     * Removes the largest key and associated value from the symbol table.
     * @throws NoSuchElementException if the symbol table is empty
     */
    void deleteMax() {
        if (isEmpty()) return;

        // if both children of root are black, set root to red
        if (!isRed(root->left) && !isRed(root->right))
            root->color = COLOR_RED;

        root = deleteMax(root);
        if (!isEmpty()) root->color = COLOR_BLACK;
        // assert check();
    }

    // delete the key-value pair with the maximum key rooted at h
    Node* deleteMax(Node* h) {
        if (isRed(h->left))
            h = rotateRight(h);

        if (h->right == nullptr)
            return nullptr;

        if (!isRed(h->right) && !isRed(h->right->left))
            h = moveRedRight(h);

        h->right = deleteMax(h->right);

        return balance(h);
    }

    /**
     * Removes the specified key and its associated value from this symbol table
     * (if the key is in this symbol table).
     *
     * @param  key the key
     */
    void innerRemove(K key) {
        // if both children of root are black, set root to red
        if (!isRed(root->left) && !isRed(root->right)) root->color = COLOR_RED;
        assignAndFreeIfNull(root, deleteKey(root, key));
        if (!isEmpty()) root->color = COLOR_BLACK;
        // assert check();
    }

    // delete the key-value pair with the given key rooted at h
    Node* deleteKey(Node* h, const K& key) {
        // assert get(h, key) != null;
        if (key < h->key)  {
            if (!isRed(h->left) && !isRed(h->left->left)) {
                h = moveRedLeft(h);
            }
            assignAndFreeIfNull(h->left, deleteKey(h->left, key));
        } else {
            if (isRed(h->left)) {
                h = rotateRight(h);
            }
            if (key == h->key && (h->right == nullptr)) {
                return nullptr;
            }
            if (!isRed(h->right) && !isRed(h->right->left)) {
                h = moveRedRight(h);
            }
            if (key == h->key) {
                Node* x = min(h->right);
                h->key = x->key;
                h->val = x->val;
                // h->val = get(h->right, min(h->right).key);
                // h->key = min(h->right).key;
                assignAndFreeIfNull(h->right, deleteMin(h->right));
            } else {
                assignAndFreeIfNull(h->right, deleteKey(h->right, key));
            }
        }
        return balance(h);
    }

    /***************************************************************************
     *  Red-black tree helper functions.
     ***************************************************************************/

    // make a left-leaning link lean to the right
    Node* rotateRight(Node* h) {
        // assert (h != null) && isRed(h->left);
        Node* x = h->left;
        h->left = x->right;
        x->right = h;
        x->color = x->right->color;
        x->right->color = COLOR_RED;
        x->size = h->size;
        h->size = size(h->left) + size(h->right) + 1;
        return x;
    }

    // make a right-leaning link lean to the left
    Node* rotateLeft(Node* h) {
        // assert (h != null) && isRed(h->right);
        Node* x = h->right;
        h->right = x->left;
        x->left = h;
        x->color = x->left->color;
        x->left->color = COLOR_RED;
        x->size = h->size;
        h->size = size(h->left) + size(h->right) + 1;
        return x;
    }

    // flip the colors of a node and its two children
    void flipColors(Node* h) {
        // h must have opposite color of its two children
        // assert (h != null) && (h->left != null) && (h->right != null);
        // assert (!isRed(h) &&  isRed(h->left) &&  isRed(h->right))
        //    || (isRed(h)  && !isRed(h->left) && !isRed(h->right));
        h->color = !h->color;
        h->left->color = !h->left->color;
        h->right->color = !h->right->color;
    }

    // Assuming that h is red and both h->left and h->left.left
    // are black, make h->left or one of its children red.
    Node* moveRedLeft(Node* h) {
        // assert (h != null);
        // assert isRed(h) && !isRed(h->left) && !isRed(h->left.left);

        flipColors(h);
        if (isRed(h->right->left)) {
            h->right = rotateRight(h->right);
            h = rotateLeft(h);
            flipColors(h);
        }
        return h;
    }

    // Assuming that h is red and both h->right and h->right.left
    // are black, make h->right or one of its children red.
    Node* moveRedRight(Node* h) {
        // assert (h != null);
        // assert isRed(h) && !isRed(h->right) && !isRed(h->right.left);
        flipColors(h);
        if (isRed(h->left->left)) {
            h = rotateRight(h);
            flipColors(h);
        }
        return h;
    }

    // restore red-black tree invariant
    Node* balance(Node* h) {
        // assert (h != null);

        if (isRed(h->right))                        h = rotateLeft(h);
        if (isRed(h->left) && isRed(h->left->left)) h = rotateRight(h);
        if (isRed(h->left) && isRed(h->right))      flipColors(h);

        h->size = size(h->left) + size(h->right) + 1;
        return h;
    }


    /***************************************************************************
     *  Utility functions.
     ***************************************************************************/

    /**
     * Returns the height of the BST (for debugging).
     * @return the height of the BST (a 1-node tree has height 0)
     */
    int height() {
        return height(root);
    }
    int height(Node* x) {
        if (x == nullptr) return -1;
        return 1 + std::max(height(x->left), height(x->right));
    }

    /***************************************************************************
     *  Ordered symbol table methods.
     ***************************************************************************/

    /**
     * Returns the smallest key in the symbol table.
     * @return the smallest key in the symbol table
     * @throws NoSuchElementException if the symbol table is empty
     */
    K* min() {
        if (isEmpty()) return nullptr;
        return min(root).key;
    }

    // the smallest key in subtree rooted at x; null if no such key
    Node* min(Node* x) {
        // assert x != null;
        if (x->left == nullptr) return x;
        else                return min(x->left);
    }

    /**
     * Returns the largest key in the symbol table.
     * @return the largest key in the symbol table
     * @throws NoSuchElementException if the symbol table is empty
     */
    K* max() {
        if (isEmpty()) return nullptr;
        return max(root).key;
    }

    // the largest key in the subtree rooted at x; null if no such key
    Node* max(Node* x) {
        // assert x != null;
        if (x->right == nullptr) return x;
        else                 return max(x->right);
    }


    /**
     * Returns the largest key in the symbol table less than or equal to {@code key}.
     * @param key the key
     * @return the largest key in the symbol table less than or equal to {@code key}
     * @throws NoSuchElementException if there is no such key
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    K* floor(const K& key) {
        if (key == nullptr) return nullptr;
        if (isEmpty()) return nullptr;
        Node* x = floor(root, key);
        if (x == nullptr) return nullptr;
        else           return x->key;
    }

    // the largest key in the subtree rooted at x less than or equal to the given key
    Node* floor(Node* x, const K& key) {
        if (x == nullptr) return nullptr;
        if (key == x->key) return x;
        if (key < x->key)  return floor(x->left, key);
        Node* t = floor(x->right, key);
        if (t != nullptr) return t;
        else           return x;
    }

    /**
     * Returns the smallest key in the symbol table greater than or equal to {@code key}.
     * @param key the key
     * @return the smallest key in the symbol table greater than or equal to {@code key}
     * @throws NoSuchElementException if there is no such key
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    K* ceiling(const K& key) {
        if (key == nullptr) return nullptr;
        if (isEmpty()) return nullptr;
        Node* x = ceiling(root, key);
        if (x == nullptr) return nullptr;
        else           return x->key;
    }

    // the smallest key in the subtree rooted at x greater than or equal to the given key
    Node* ceiling(Node* x, const K& key) {
        if (x == nullptr) return nullptr;
        if (key == x->key) return x;
        if (x->key < key)  return ceiling(x->right, key);
        Node* t = ceiling(x->left, key);
        if (t != nullptr) return t;
        else           return x;
    }

    /**
     * Return the kth smallest key in the symbol table.
     * @param k the order statistic
     * @return the {@code k}th smallest key in the symbol table
     * @throws IllegalArgumentException unless {@code k} is between 0 and
     *     <em>n</em>–1
     */
    K* select(int k) {
        if (k < 0 || k >= size()) {
            return nullptr;
        }
        Node x = select(root, k);
        return x->key;
    }

    // the key of rank k in the subtree rooted at x
    Node* select(Node* x, int k) {
        // assert x != null;
        // assert k >= 0 && k < size(x);
        int t = size(x->left);
        if      (t > k) return select(x->left,  k);
        else if (t < k) return select(x->right, k-t-1);
        else            return x;
    }

    /**
     * Return the number of keys in the symbol table strictly less than {@code key}.
     * @param key the key
     * @return the number of keys in the symbol table strictly less than {@code key}
     * @throws IllegalArgumentException if {@code key} is {@code null}
     */
    int rank(const K& key) {
        if (key == nullptr) return -1;
        return rank(key, root);
    }

    // number of keys less than key in the subtree rooted at x
    int rank(const K& key, Node* x) {
        if (x == nullptr) return 0;
        if      (key < x->key) return rank(key, x->left);
        else if (x->key < key) return 1 + size(x->left) + rank(key, x->right);
        else              return size(x->left);
    }

    /***************************************************************************
     *  Range count and range search->
     ***************************************************************************/


    /**
     * Returns the number of keys in the symbol table in the given range.
     *
     * @param  lo minimum endpoint
     * @param  hi maximum endpoint
     * @return the number of keys in the sybol table between {@code lo}
     *    (inclusive) and {@code hi} (inclusive)
     * @throws IllegalArgumentException if either {@code lo} or {@code hi}
     *    is {@code null}
     */
    int size(const K& lo, const K& hi) {
        if (lo == nullptr) return 0;
        if (hi == nullptr) return 0;

        if (hi < lo) return 0;
        if (containsKey(hi)) return rank(hi) - rank(lo) + 1;
        else              return rank(hi) - rank(lo);
    }


    /***************************************************************************
     *  Check integrity of red-black tree data structure.
     ***************************************************************************/
    bool check() {
        if (!isBST())            std::cout << "Not in symmetric order\n";
        if (!isSizeConsistent()) std::cout << "Subtree counts not consistent\n";
        //if (!isRankConsistent()) std::cout << "Ranks not consistent\n";
        if (!is23())             std::cout << "Not a 2-3 tree\n";
        if (!isBalanced())       std::cout << "Not balanced\n";
        return isBST() && isSizeConsistent() && is23() && isBalanced();
    }

    // does this binary tree satisfy symmetric order?
    // Note: this test also ensures that data structure is a binary tree since order is strict
    bool isBST() {
        return isBST(root, nullptr, nullptr);
    }

    // is the tree rooted at x a BST with all keys strictly between min and max
    // (if min or max is null, treat as empty constraint)
    // Credit: Bob Dondero's elegant solution
    bool isBST(Node* x, K* min, K* max) {
        if (x == nullptr) return true;
        // TODO: port these two lines
        //if (min != nullptr && x->key.compareTo(min) <= 0) return false;
        //if (max != nullptr && x->key.compareTo(max) >= 0) return false;
        return isBST(x->left, min, x->key) && isBST(x->right, x->key, max);
    }

    // are the size fields correct?
    bool isSizeConsistent() { return isSizeConsistent(root); }
    bool isSizeConsistent(Node* x) {
        if (x == nullptr) return true;
        if (x->size != size(x->left) + size(x->right) + 1) return false;
        return isSizeConsistent(x->left) && isSizeConsistent(x->right);
    }

    /*
    // check that ranks are consistent
    bool isRankConsistent() {
        for (int i = 0; i < size(); i++)
            if (i != rank(select(i))) return false;
        for (K* key : keys())
            if (key.compareTo(select(rank(key))) != 0) return false;
        return true;
    }
    */

    // Does the tree have no red right links, and at most one (left)
    // red links in a row on any path?
    bool is23() { return is23(root); }
    bool is23(Node* x) {
        if (x == nullptr) return true;
        if (isRed(x->right)) return false;
        if (x != root && isRed(x) && isRed(x->left))
            return false;
        return is23(x->left) && is23(x->right);
    }

    // do all paths from root to leaf have same number of black edges?
    bool isBalanced() {
        int black = 0;     // number of black links on path from root to min
        Node x = root;
        while (x != nullptr) {
            if (!isRed(x)) black++;
            x = x->left;
        }
        return isBalanced(root, black);
    }

    // does every path from the root to a leaf have the given number of black links?
    bool isBalanced(Node* x, int black) {
        if (x == nullptr) return black == 0;
        if (!isRed(x)) black--;
        return isBalanced(x->left, black) && isBalanced(x->right, black);
    }



    // Inserts a key only if it's not already present
    bool add(K key, const int tid=0) {
        return ofwf::updateTx<bool>([=] () {
            return innerPut(key,key);
        });
    }

    // Returns true only if the key was present
    bool remove(K key, const int tid=0) {
        return ofwf::updateTx<bool>([=] () {
            V notused;
            bool retval = innerGet(key,notused,false);
            if (retval) innerRemove(key);
            return retval;
        });
    }

    bool contains(K key, const int tid=0) {
        return ofwf::readTx<bool>([=] () {
            V notused;
            return innerGet(key,notused,false);
        });
    }

    void addAll(K** keys, int size, const int tid=0) {
        for (int i = 0; i < size; i++) add(*keys[i], tid);
    }

    static std::string className() { return "OF-WF-RedBlackTree"; }

};

#endif   // _OF_WF_RED_BLACK_BST_H_
