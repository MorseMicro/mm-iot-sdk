/*
 * Copyright 2022-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * Morse Micro C++ example application
 *
 * This file demonstrates how to build and include C++ files with the @c MM-IoT-SDK.
 * The application demonstrates how to use C++ standard library features such as @c iostream
 * and how to use C++ exceptions.  C++ exceptions are not enabled by default in the @c  MM-IoT-SDK
 * framework by default due to the high cost in terms of code size of the default exception
 * handlers. You can enable support for exception handling by setting @c CSPECS to @c nosys.specs
 * as shown in @c app.mk.  You can exclude this change to reduce the code size with the
 * implication being that any exception will simply exit the application.
 */

#include <iostream>
#include "mmosal.h"

/**
 * The @c ClassDemo class
 */
class ClassDemo
{
 public:
    /**
     * Constructor for @c ClassDemo
     * @param p_comment A string to print with each message
     */
    explicit ClassDemo(const char *p_comment)
    {
        std::cout << "ClassDemo constructor is running: " << p_comment << std::endl;
        count = 0;
        comment = p_comment;
    }

    /**
     * Destructor for @c ClassDemo
     */
    ~ClassDemo()
    {
        std::cout << "ClassDemo destructor is running: " << comment << std::endl;
    }

    /**
     * Getter for count value
     * @return current count value
     */
    int GetDemoCount()
    {
        return count;
    }

    /**
     * Increment count value
     * @return count value after increment
     */
    int IncrementDemoCount()
    {
        count++;
        return count;
    }

    /**
     * Set the count value
     * @param value The value to set it to
     */
    void SetDemoCount(int value)
    {
        count = value;
    }

 private:
    int count;
    const char *comment;
};

/**
 * A global instance of the @c ClassDemo class
 */
ClassDemo global_demo("global demo class");

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed.
 */
extern "C" void app_init(void)
{
    printf("\n\nC++ Demo Example (Built " __DATE__ " " __TIME__ ")\n\n");

    while (true)
    {
        global_demo.IncrementDemoCount();
        int demo_loop_count = global_demo.GetDemoCount();

        ClassDemo demo("local demo class");
        demo.SetDemoCount(demo_loop_count);
        int old_count = demo.IncrementDemoCount();
        int current_count = demo.GetDemoCount();

        std::cout << "Demo cout in loop " << demo_loop_count
                  << "... next count will be " << old_count
                  << "/" << current_count << std::endl;

#ifdef __EXCEPTIONS
        /* Note: Exceptions are not supported by libc-nano.
         * See app.mk on how to enable exception support using libc-nosys.
         * If you use exceptions in libc-nano, the application
         * will simply exit when you call throw().
         *
         * Also note that exceptions come with a huge cost and
         * can add 30k to over 100k to your application code even if
         * you use no C++ code (As exceptions like divide by 0 can be
         * thrown by C code too). So use libc-nano and avoid using
         * exception handlers in your C++ code if size is a constraint.
         */
        try
        {
            if (demo_loop_count % 2 == 0)
            {
                std::cout << "About to throw..." << std::endl;
                throw("count was even");
            }
            else
            {
                std::cout << "Odd"  << std::endl;;
            }
        }
        catch(const char *reason)
        {
            std::cout << "Exception: " << reason  << std::endl;
        }
#endif

        mmosal_task_sleep(5000);
    }
}
