#include "CommonMath.h"

static bool seedInitialized = false;

int CommonMath::random(int min, int max)
{
    if (!seedInitialized)
    {
        srand((int)time(0));
        seedInitialized = true;
    }
    return rand() % (max - min + 1) + min;
}

double CommonMath::randomF(double min, double max)
{
    if (!seedInitialized)
    {
        srand((int)time(0));
        seedInitialized = true;
    }
    double f = (double)rand() / RAND_MAX;
    return min + f * (max - min);
}
