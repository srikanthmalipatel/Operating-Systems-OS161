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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <current.h>
#include <synch.h>
#include <kern/secret.h>
#include <spinlock.h>
/*
 * Called by the driver during initialization.
 */


static struct lock *lock_quad0;
static struct lock *lock_quad1;
static struct lock *lock_quad2;
static struct lock *lock_quad3;
/*static struct cv   *cv_quad1;
static struct cv   *cv_quad2;
static struct cv   *cv_quad3;
static struct cv   *cv_quad0;*/


void
stoplight_init() {
	lock_quad0 = lock_create("quad0");
	lock_quad1 = lock_create("quad1");
	lock_quad2 = lock_create("quad2");
	lock_quad3 = lock_create("quad3");
	return;
}

/*
 * Called by the driver during teardown.
 */


void stoplight_cleanup() {
	lock_destroy(lock_quad0);
	lock_destroy(lock_quad1);
	lock_destroy(lock_quad2);
	lock_destroy(lock_quad3);
	return;
}

void lock_quad(uint32_t direction)
{
 // kprintf_n("%s wants to acquire lock %d \n", curthread->t_name,direction);
	 if(direction == 0)
	 	lock_acquire(lock_quad0);
	 else if(direction == 1)
	 	lock_acquire(lock_quad1);
	 else if(direction == 2)
	 	lock_acquire(lock_quad2);
	 else if(direction == 3)
	 	lock_acquire(lock_quad3);
//	kprintf_n("%s acquired lock  %d \n", curthread->t_name,direction);

}


void release_quad(uint32_t direction)
{
	
	 if(direction == 0)
	 	lock_release(lock_quad0);
	 else if(direction == 1)
	 	lock_release(lock_quad1);
	 else if(direction == 2)
	 	lock_release(lock_quad2);
	 else if(direction == 3)
	 	lock_release(lock_quad3);
//	kprintf_n("%s released lock %d\n", curthread->t_name,direction);

}
void
turnright(uint32_t direction, uint32_t index)
{
//	(void)direction;
//	(void)index;
	/*
	 * Implement this function.
	 */
//	kprintf_n("%s entered at  %d and wants to turn right\n", curthread->t_name,direction);
	lock_quad(direction);
	inQuadrant(direction, index);
	leaveIntersection(index);
	release_quad(direction);


	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{

/// USING CIRCULAR WAIT..ACCESS RESOURCES IN A PARTICULAR ORDER, IN THIS CASE, WE ARE ACCESSING THEM IN INCREASING ORDER, DONE TO AVOID DEADLOCK
	int first_quad, second_quad;
	second_quad = (direction + 3 )%4;
	first_quad = direction ;
	int quad1, quad2;

	if(first_quad > second_quad)
	{
		quad1 = second_quad;
		quad2 = first_quad;
	}
	else
	{
		quad1 = first_quad;
		quad2 = second_quad;
	
	}
	lock_quad(quad1);
	lock_quad(quad2);
	inQuadrant(first_quad,index);
	inQuadrant(second_quad,index);
	release_quad(first_quad);
	leaveIntersection(index);
	release_quad(second_quad);
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	
	int first_quad = direction;
	int second_quad = (direction + 3)%4;
	int third_quad = (direction + 6)%4;

	int quad1,quad2,quad3;

	if(first_quad>second_quad)
	{
		if(second_quad > third_quad) // first > second > third
		{
			quad1 = third_quad;
			quad2 = second_quad;  
			quad3 = first_quad;
		}
		else
		{
			if(first_quad > third_quad) // first > third > second
			{
				quad1= second_quad;
				quad2 = third_quad;
				quad3 = first_quad;
			
			}
			else // third > first > second
			{
				quad1 = second_quad;
				quad2 = first_quad;
				quad3 = third_quad;
			}
		
		}
	
	}
	else
	{
		if(first_quad > third_quad) // second > first > third
		{
			quad1 = third_quad;
			quad2 = first_quad;
			quad3 = second_quad;
		}
		else
		{
			if(second_quad > third_quad) // second> third > first
			{
				quad1 = first_quad;
				quad2 = third_quad;
				quad3 = second_quad;
			
			}
			else // third> second> first
			{
				quad1 = first_quad;
				quad2 = second_quad;
				quad3 = third_quad;
			
			}
		
		}
	
	}


	lock_quad(quad1);
	lock_quad(quad2);
	lock_quad(quad3);
	inQuadrant(first_quad,index);
	inQuadrant(second_quad,index);
	inQuadrant(third_quad,index);
	release_quad(first_quad);
	release_quad(second_quad);
	leaveIntersection(index);
	release_quad(third_quad);


	return;
}
