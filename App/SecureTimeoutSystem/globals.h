#ifndef GLOBALS_H
#define GLOBALS_H

/* Define boolean values */
typedef short int my_bool;
#define true 1
#define false 0

/* Global variables */
extern volatile int userActivity;
extern volatile int userActivityDetection;
extern volatile int suspiciousActivity;
extern volatile int suspiciousActivityDetection;

#endif /* GLOBALS_H */
