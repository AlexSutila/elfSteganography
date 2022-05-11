/*
    This is the source code which was compiled to the source.o file that was
        embedded in frog.bmp

    It simply calculates the n'th fibbonacci number using dynamic programming
        rather than recursion, because as of the time I write this, other
        functions can not be called from within functions called from the
        extracted binary.

	gcc -c source.c
*/

int fib(int n)
{
    int f[n + 2], i;
    f[0] = 0;
    f[1] = 1;
    for (int i = 2; i <= n; i++)
    {
        f[i] = f[i - 1] + f[i - 2];
    }
    return f[n];
}
