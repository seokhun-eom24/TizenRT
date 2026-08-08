/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <sched.h>
#include <unistd.h>

#if !defined(CONFIG_BUILD_KERNEL) && defined(CONFIG_SMP) && CONFIG_SMP_NCPUS > 1
static int hello_task_exit(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	return 0;
}

static int hello_task_sleep(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	while (1) {
		sleep(10);
	}

	return 0;
}

static int hello_task_loop(int cpu)
{
	cpu_set_t cpuset;
	pid_t pid;

	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0) {
		return -1;
	}

	while (1) {
		pid = task_create("hello_exit", SCHED_PRIORITY_DEFAULT, 1024,
				  hello_task_exit, NULL);
		if (pid < 0) {
			return -1;
		}

		sched_yield();
		task_delete(pid);

		pid = task_create("hello_sleep", SCHED_PRIORITY_DEFAULT, 1024,
				  hello_task_sleep, NULL);
		if (pid < 0) {
			return -1;
		}

		task_delete(pid);
	}
}

static int hello_task_cpu0(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	return hello_task_loop(0);
}

static int hello_task_cpu1(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	return hello_task_loop(1);
}
#endif

/****************************************************************************
 * hello_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int hello_main(int argc, char *argv[])
#endif
{
#if !defined(CONFIG_BUILD_KERNEL) && defined(CONFIG_SMP) && CONFIG_SMP_NCPUS > 1
	pid_t cpu0_pid;
	pid_t cpu1_pid;

	cpu0_pid = task_create("hello_cpu0", SCHED_PRIORITY_DEFAULT, 1024,
				   hello_task_cpu0, NULL);
	if (cpu0_pid < 0) {
		return -1;
	}

	cpu1_pid = task_create("hello_cpu1", SCHED_PRIORITY_DEFAULT, 1024,
				   hello_task_cpu1, NULL);
	if (cpu1_pid < 0) {
		task_delete(cpu0_pid);
		return -1;
	}

	while (1) {
		sleep(10);
	}
#else
	return -1;
#endif

	return 0;
}
