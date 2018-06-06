/*
 * compiler.h
 *
 *  Created on: Jun 6, 2018
 *      Author: luxq
 */

#ifndef SRC_COMPILER_H_
#define SRC_COMPILER_H_

#define BUILD_BUG_ON(condition)  ((void)sizeof(char[1-2*!!(condition)]))

/*
 * for example: BUILD_CHECK_SIZE_EQUAL(sizeof(struct a), 2048)
 */
#define BUILD_CHECK_SIZE_EQUAL(a, s) BUILD_BUG_ON((a)-(s))

/*
 * for example: BUILD_CHECK_SIZE_ALIGN(ENV_PARTATION_OFFSET, 0xff)
 */
#define BUILD_CHECK_SIZE_ALIGN(a, align) BUILD_BUG_ON((a)%(align))

#endif /* SRC_COMPILER_H_ */
