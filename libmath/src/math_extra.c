int square(int x)
{
    return x * x;
}

int cube(int x)
{
    return x * x * x;
}

int factorial(int x)
{
    if (x <= 1) return 1;
    int result = 1;
    for (int i = 2; i <= x; i++)
        result *= i;
    return result;
}
