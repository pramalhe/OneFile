#ifndef _PERSISTENT_TM_LINKED_LIST_SETBYREF_H_
#define _PERSISTENT_TM_LINKED_LIST_SETBYREF_H_

#include <string>


/**
 * <h1> A Linked List Set meant to be used with PTMs </h1>
 */
template<typename K, typename TM, template <typename> class TMTYPE>
class TMLinkedListSetByRef {

private:
    struct Node {
        TMTYPE<K>     key;
        TMTYPE<Node*> next {nullptr};
        Node(const K& key) : key{key} { }
        Node(){ }
    };

    alignas(128) TMTYPE<Node*>  head {nullptr};
    alignas(128) TMTYPE<Node*>  tail {nullptr};


public:
    TMLinkedListSetByRef() {
        TM::template updateTx<bool>([=] () {
            Node* lhead = TM::template tmNew<Node>();
            Node* ltail = TM::template tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
            return true; // Needed for CX
        });
    }

    ~TMLinkedListSetByRef() {
        TM::template updateTx<bool>([=] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                TM::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            TM::tmDelete(prev);
            TM::tmDelete(tail.pload());
            return true; // Needed for CX
        });
    }

    static std::string className() { return TM::className() + "-LinkedListSet"; }

    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(K key) {
        bool retval = false;
        TM::template updateTx<bool>([&] () {
            Node *prev, *node;
            find(key, prev, node);
            retval = !(node != tail && key == node->key);
            if (!retval) return;
            Node* newNode = TM::template tmNew<Node>(key);
            prev->next = newNode;
            newNode->next = node;
        });
        return retval;
    }

    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(K key) {
        bool retval = false;
        TM::template updateTx<bool>([&] () {
            Node *prev, *node;
            find(key, prev, node);
            retval = (node != tail && key == node->key);
            if (!retval) return;
            prev->next = node->next;
            TM::tmDelete(node);
        });
        return retval;
    }

    /*
     * Returns true if it finds a node with a matching key
     */
    bool contains(K key) {
        bool retval = false;
        TM::template readTx<bool>([&] () {
            Node *prev, *node;
            find(key, prev, node);
            retval = (node != tail && key == node->key);
        });
        return retval;
    }

    void find(const K& lkey, Node*& prev, Node*& node) {
        Node* ltail = tail;
        for (prev = head; (node = prev->next) != ltail; prev = node) {
            if ( !(node->key < lkey) ) break;
        }
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size) {
        for (int i = 0; i < size; i++) add(*keys[i]);
        return true;
    }
};

#endif /* _PERSISTENT_TM_LINKED_LIST_SETBYREF_H_ */
