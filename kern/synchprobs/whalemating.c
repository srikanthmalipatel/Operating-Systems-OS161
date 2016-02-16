/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */


static uint32_t male_count;
static struct lock *commonLock;
static uint32_t female_count;
static uint32_t matchmaker_count;
static struct cv *cv_male;
static struct cv *cv_female;
static struct cv *cv_matchmaker;

void whalemating_init() {
	commonLock = lock_create("commonLock");
	cv_male = cv_create("male cv");
	cv_female = cv_create("female cv");
	cv_matchmaker = cv_create("matchmaker cv");

	male_count = 0;
	female_count = 0;
	matchmaker_count = 0;
	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	lock_destroy(commonLock);
	cv_destroy(cv_male);
	cv_destroy(cv_female);
	cv_destroy(cv_matchmaker);
	return;
}

void
male(uint32_t index)
{
//	(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */
	male_start(index);
	lock_acquire(commonLock);
	male_count++;

	if(female_count > 0 && matchmaker_count > 0)
	{
		female_count--;
		matchmaker_count--;
		male_count--;

		cv_signal(cv_female,commonLock);
		cv_signal(cv_matchmaker,commonLock);
	
	
	}
	else
	{
		cv_wait(cv_male,commonLock);
	
	}
	male_end(index);
	lock_release(commonLock);
	return;
}

void
female(uint32_t index)
{
//	(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */

	 female_start(index);
	 lock_acquire(commonLock);
	 female_count++;
	 
	 if(male_count > 0 && matchmaker_count > 0)
	 {
	 	male_count--;
	 	female_count--;
	 	matchmaker_count --;

	 	cv_signal(cv_male,commonLock);
	 	cv_signal(cv_matchmaker,commonLock);
	 
	 }
	 else
	 {
	 	cv_wait(cv_female,commonLock);
	 
	 
	 }
	 female_end(index);
	 lock_release(commonLock);
	return;
}

void
matchmaker(uint32_t index)
{
//	(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */

	 matchmaker_start(index);

	 lock_acquire(commonLock);
	 matchmaker_count++;

	 if(male_count > 0 && female_count > 0)
	 {
	 	male_count--;
	 	female_count--;
	 	matchmaker_count--;

	 	cv_signal(cv_male,commonLock);
	 	cv_signal(cv_female,commonLock);
	 
	 }
	 else
	 {
	 	cv_wait(cv_matchmaker,commonLock);
	 }
	matchmaker_end(index);
	 lock_release(commonLock);
	return;
}
