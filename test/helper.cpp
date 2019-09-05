#include "test_macros.hpp"
#include <matrix/helper_functions.hpp>

using namespace matrix;

int main()
{
    // general wraps
    TEST(fabs(wrap(4., 0., 10.) - 4.) < FLT_EPSILON);
    TEST(fabs(wrap(4., 0., 1.)) < FLT_EPSILON);
    TEST(fabs(wrap(-4., 0., 10.) - 6.) < FLT_EPSILON);
    TEST(fabs(wrap(-18., 0., 10.) - 2.) < FLT_EPSILON);
    TEST(fabs(wrap(-1.5, 3., 5.) - 4.5) < FLT_EPSILON);
    TEST(fabs(wrap(15.5, 3., 5.) - 3.5) < FLT_EPSILON);
    TEST(fabs(wrap(-1., 30., 40.) - 39.) < FLT_EPSILON);
    TEST(fabs(wrap(-8000., -555., 1.) - (-216.)) < FLT_EPSILON);
    TEST(fabs(wrap(0., 0., 360.)) < FLT_EPSILON);
    TEST(fabs(wrap(0. - FLT_EPSILON, 0., 360.) - (360. - FLT_EPSILON)) < FLT_EPSILON);
    TEST(fabs(wrap(0. + FLT_EPSILON, 0., 360.) - FLT_EPSILON) < FLT_EPSILON);
    TEST(fabs(wrap(360., 0., 360.)) < FLT_EPSILON);
    TEST(fabs(wrap(360. - FLT_EPSILON, 0., 360.) - (360. - FLT_EPSILON)) < FLT_EPSILON);
    TEST(fabs(wrap(360. + FLT_EPSILON, 0., 360.) - FLT_EPSILON) < FLT_EPSILON);
    TEST(!is_finite(wrap(1000., 0., .01)));

    // wrap pi
    TEST(fabs(wrap_pi(0.)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(4.) - (4. - M_TWOPI)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(-4.) - (-4. + M_TWOPI)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(3.) - (3.)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(100.) - (100. - 32. * M_PI)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(-100.) - (-100. + 32. * M_PI)) < FLT_EPSILON);
    TEST(fabs(wrap_pi(-101.) - (-101. + 32. * M_PI)) < FLT_EPSILON);
    TEST(!is_finite(wrap_pi(NAN)));

    // wrap 2pi
    TEST(fabs(wrap_2pi(0.)) < FLT_EPSILON);
    TEST(fabs(wrap_2pi(-4.) - (-4. + 2. * M_PI)) < FLT_EPSILON);
    TEST(fabs(wrap_2pi(3.) - (3.)) < FLT_EPSILON);
    TEST(fabs(wrap_2pi(200.) - (200. - 31. * M_TWOPI)) < FLT_EPSILON);
    TEST(fabs(wrap_2pi(-201.) - (-201. + 32. * M_TWOPI)) < FLT_EPSILON);
    TEST(!is_finite(wrap_2pi(NAN)));

    Vector3f a(1, 2, 3);
    Vector3f b(4, 5, 6);
    TEST(!isEqual(a, b));
    TEST(isEqual(a, a));

    TEST(isEqualF(1., 1.));
    TEST(!isEqualF(1., 2.));
    return 0;
}

/* vim: set et fenc=utf-8 ff=unix sts=0 sw=4 ts=4 : */
