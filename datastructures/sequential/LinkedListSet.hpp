#ifndef _SEQUENTIAL_LINKED_LIST_SET_H_
#define _SEQUENTIAL_LINKED_LIST_SET_H_

#include <string>

/**
 * <h1> A sequential implementation of La inked List Set </h1>
 *
 * This is meant to be used by the Universal Constructs
 *
 */
template<typename K>
class LinkedListSet {

private:

    struct Node {
        K     key;
        Node* next{nullptr};
        Node(const K& key) : key{key}, next{nullptr} { }
        Node(){ }
    };

    Node*  head {nullptr};
    Node*  tail {nullptr};


public:
    LinkedListSet() {
		Node* lhead = new Node();
		Node* ltail = new Node();
		head = lhead;
		head->next = ltail;
		tail = ltail;

    }


    // Universal Constructs need a copy constructor on the underlying data structure
    LinkedListSet(const LinkedListSet& other) {
        head = new Node();
        Node* node = head;
        Node* onode = other.head->next;
        while (onode != other.tail) {
            node->next = new Node(onode->key);
            node = node->next;
            onode = onode->next;
        }
        tail = new Node();
        node->next = tail;
    }


    ~LinkedListSet() {
		// Delete all the nodes in the list
		Node* prev = head;
		Node* node = prev->next;
		while (node != tail) {
			delete prev;
			prev = node;
			node = node->next;
		}
		delete prev;
		delete tail;
    }


    static std::string className() { return "LinkedListSet"; }


    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(const K& key) {
        Node *prev, *node;
        find(key, prev, node);
        bool retval = !(node != tail && key == node->key);
        if (!retval) return retval;
        Node* newNode = new Node(key);
        prev->next = newNode;
        newNode->next = node;
        return retval;
    }


    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(const K& key) {
        Node *prev, *node;
        find(key, prev, node);
        bool retval = (node != tail && key == node->key);
        if (!retval) return retval;
        prev->next = node->next;
        delete node;
        return retval;
    }


    /*
     * Returns true if it finds a node with a matching key
     */
    bool contains(const K& key) {
        Node *prev, *node;
        find(key, prev, node);
        return (node != tail && key == node->key);
    }

    void find(const K& key, Node*& prev, Node*& node) {
        for (prev = head; (node = prev->next) != tail; prev = node){
            if ( !(node->key < key) ) break;
        }
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size) {
        bool retval = false;
        for (int i = 0; i < size; i++) {
            Node *prev, *node;
            find(*keys[i], prev, node);
            retval = !(node != tail && *keys[i] == node->key);
            if (retval) {
                Node* newNode = new Node(*keys[i]);
                prev->next = newNode;
                newNode->next = node;
            }
        }
        return true;
    }
};

#endif /* _SEQUENTIAL_LINKED_LIST_SET_H_ */
