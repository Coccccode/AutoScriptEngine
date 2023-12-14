#include "CommonMath.h"

int CommonMath::random(int min, int max)
{
	srand((int)time(0));
	return rand() % (max - min) + min;
}

double CommonMath::randomF(double min, double max)
{
	return 0.0;
}
