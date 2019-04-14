#ifndef _PERSISTENT_TM_RESIZABLE_HASH_MAPBYREF_H_
#define _PERSISTENT_TM_RESIZABLE_HASH_MAPBYREF_H_

#include <string>

/**
 * <h1> A Resizable Hash Map for PTMs </h1>
 */
template<typename K, typename V, typename TM, template <typename> class TMTYPE>
class TMHashMapByRef {

private:
    struct Node {
        TMTYPE<K>     key;
        TMTYPE<V>     val;
        TMTYPE<Node*> next {nullptr};
        Node(const K& k, const V& v) : key{k}, val{v} { } // Copy constructor for k and value
        Node() {}
    };


    TMTYPE<uint64_t>                    capacity;
    TMTYPE<uint64_t>                    sizeHM = 0;
    //TMTYPE<double>					loadFactor = 0.75;
    static constexpr double             loadFactor = 0.75;
    alignas(128) TMTYPE<TMTYPE<Node*>*> buckets;      // An array of pointers to Nodes


public:
    TMHashMapByRef(uint64_t capacity=4) : capacity{capacity} {
		buckets = (TMTYPE<Node*>*)TM::pmalloc(capacity*sizeof(TMTYPE<Node*>));
		for (int i = 0; i < capacity; i++) buckets[i]=nullptr;
    }


    ~TMHashMapByRef() {
		for(int i = 0; i < capacity; i++){
			Node* node = buckets[i];
			while (node!=nullptr) {
				Node* next = node->next;
				TM::tmDelete(node);
				node = next;
			}
		}
		TM::pfree(buckets);
    }


    static std::string className() { return TM::className() + "-HashMap"; }


    void rebuild() {
        uint64_t newcapacity = 2*capacity;
        //printf("increasing capacity to %d\n", newcapacity);
        TMTYPE<Node*>* newbuckets = (TMTYPE<Node*>*)TM::pmalloc(newcapacity*sizeof(TMTYPE<Node*>));
        for (int i = 0; i < newcapacity; i++) newbuckets[i] = nullptr;
        for (int i = 0; i < capacity; i++) {
            Node* node = buckets[i];
            while(node!=nullptr){
                Node* next = node->next;
                auto h = std::hash<K>{}(node->key) % newcapacity;
                node->next = newbuckets[h];
                newbuckets[h] = node;
                node = next;
            }
        }
        TM::pfree(buckets);
        buckets = newbuckets;
        capacity = newcapacity;
    }


    /*
     * Adds a node with a key if the key is not present, otherwise replaces the value.
     * If saveOldValue is set, it will set 'oldValue' to the previous value, iff there was already a mapping.
     *
     * Returns true if there was no mapping for the key, false if there was already a value and it was replaced.
     */
    bool innerPut(const K& key, const V& value, V& oldValue, const bool saveOldValue) {
    	//printf("innerPut %d %d %f\n", sizeHM.pload(), capacity.pload(), loadFactor.pload()*capacity.pload());
        if (sizeHM.pload() > capacity.pload()*loadFactor) rebuild();
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        Node* prev = node;
        while (true) {
            if (node == nullptr) {
                Node* newnode = TM::template tmNew<Node>(key,value);
                //Node* newnode = TM::template tmNew<Node>();
                //newnode->key = key;
                //newnode->val = value;
                //newnode->next = nullptr;
                if (node == prev) {
                    buckets[h] = newnode;
                } else {
                    prev->next = newnode;
                }
                sizeHM=sizeHM+1;
                return true;  // New insertion
            }
            if (key == node->key) {
                if (saveOldValue) oldValue = node->val; // Makes a copy of V
                node->val = value;
                return false; // Replace value for existing key
            }
            prev = node;
            node = node->next;
        }
    }


    /*
     * Removes a key and its mapping.
     * Saves the value in 'oldvalue' if 'saveOldValue' is set.
     *
     * Returns returns true if a matching key was found
     */
    bool innerRemove(const K& key, V& oldValue, const bool saveOldValue) {
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        Node* prev = node;
        while (true) {
            if (node == nullptr) return false;
            if (key == node->key) {
                if (saveOldValue) oldValue = node->val; // Makes a copy of V
                if (node == prev) {
                    buckets[h] = node->next;
                } else {
                    prev->next = node->next;
                }
                sizeHM=sizeHM-1;
                TM::tmDelete(node);
                return true;
            }
            prev = node;
            node = node->next;
        }
    }


    /*
     * Returns true if key is present. Saves a copy of 'value' in 'oldValue' if 'saveOldValue' is set.
     */
    bool innerGet(const K& key, V& oldValue, const bool saveOldValue) {
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        while (true) {
            if (node == nullptr) return false;
            if (key == node->key) {
                if (saveOldValue) oldValue = node->val; // Makes a copy of V
                return true;
            }
            node = node->next;
        }
    }


    //
    // Set methods for running the usual tests and benchmarks
    //

    // Inserts a key only if it's not already present
    bool add(const K& key) {
        bool retval = false;
        TM::template updateTx<bool>([&] () {
            V notused;
            retval = innerPut(key,key,notused,false);
        });
        return retval;
    }

    // Returns true only if the key was present
    bool remove(const K& key) {
        bool retval = false;
        TM::template updateTx<bool>([&] () {
            V notused;
            retval = innerRemove(key,notused,false);
        });
        return retval;
    }

    bool contains(const K& key) {
        bool retval = false;
        TM::template readTx<bool>([&] () {
            V notused;
            retval = innerGet(key,notused,false);
        });
        return retval;
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size) {
        for (int i = 0; i < size; i++) add(*keys[i]);
        return true;
    }
};

#endif /* _PERSISTENT_TM_RESIZABLE_HASH_MAPByRef_H_ */
