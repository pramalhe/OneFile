/* 
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 *
 * Created on April 20, 2017, 1:09 PM
 */

#ifndef ERRORS_H
#define	ERRORS_H

#include <iostream>
#include <string>
#include <unistd.h>

#ifndef error
#define error(s) { \
    std::cout<<"ERROR: "<<s<<" (at "<<__FILE__<<"::"<<__FUNCTION__<<":"<<__LINE__<<")"<<std::endl; \
    exit(-1); \
}
#endif

//__attribute__((always_inline))
//void error(std::string s) {
//    std::cout<<"ERROR: "<<s<<" (at "<<__FILE__<<"::"<<__FUNCTION__<<":"<<__LINE__<<")"<<std::endl;
//    exit(-1);
//}

#endif	/* ERRORS_H */

