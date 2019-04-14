# Queues #

This folder contains multiple multi-producer-multi-consumer queue implementations, all of them with integrated memory reclamation having the same progress condition:

- FAAArrayQueue: Memory unbounded, lock-free, one array per node, hazard pointers
http://...

- LCRQueue: Memory unbounded, lock-free, one array per node, hazard pointers, can re-use entries in some situations
http://

- OFLFLinkedListqueue: Memory unbounded, lock-free, one entry per node, hazard eras
Uses OneFile STM (Lock-Free)

- OFWFLinkedListqueue: Memory unbounded, wait-free bounded, one entry per node, hazard eras
Uses OneFile STM (Wait-Free)

- SimQueue: Memory unbounded, wait-free bounded, one entry per node, modified hazard pointers
http://

- TurnQueue: Memory unbounded, wait-free bounded, one entry per node, hazard pointers
http:// 
