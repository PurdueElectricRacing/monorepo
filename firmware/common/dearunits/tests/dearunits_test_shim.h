#ifndef DEARUNITS_TEST_SHIM_H
#define DEARUNITS_TEST_SHIM_H

typedef void (*dearunits_check_fn)(float actual, float expected, float tolerance, const char *expression, int line);

void dearunits_test_basics(dearunits_check_fn check);
void dearunits_test_drivetrain(dearunits_check_fn check);
void dearunits_test_battery(dearunits_check_fn check);
void dearunits_test_dynamics(dearunits_check_fn check);
void dearunits_test_brakes(dearunits_check_fn check);
void dearunits_test_signal_helpers(dearunits_check_fn check);

#endif // DEARUNITS_TEST_SHIM_H
