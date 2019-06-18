/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include "cachelab.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_stride(int M, int N, int A[N][M], int B[M][N], int stride);
void trans_64x64(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	if (M == 32 && N == 32)
		trans_stride(M, N, A, B, 8);

	if (M == 64 && N == 64)
		trans_64x64(M, N, A, B);

	if (M == 61 && N == 67)
		trans_stride(M, N, A, B, 16);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, tmp;

	for (i = 0; i < N; i++) {
		for (j = 0; j < M; j++) {
			tmp = A[i][j];
			B[j][i] = tmp;
		}
	}
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
	/* Register your solution function */
	registerTransFunction(transpose_submit, transpose_submit_desc);

	/* Register any additional transpose functions */
	registerTransFunction(trans, trans_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
	int i, j;

	for (i = 0; i < N; i++) {
		for (j = 0; j < M; ++j) {
			if (A[i][j] != B[j][i]) {
				return 0;
			}
		}
	}
	return 1;
}

/*
 * trans_stride - This transpose function divides A into square
 *     submatrices of order _stride_ and transposes each submatrix.
 *     By checking ranges of rows and columns, this function supports
 *     matrices with arbitrary dimensions.
 */
void trans_stride(int M, int N, int A[N][M], int B[M][N], int stride)
{
	int i, j, k, l, tmp;

	for (i = 0; i < M; i += stride)
		for (j = 0; j < N; j += stride)
			for (k = j; k < MIN(j + stride, N); k++) {
				for (l = i; l < MIN(i + stride, M); l++) {
					if (k == l)
						tmp = A[k][l];
					else
						B[l][k] = A[k][l];
				}

				if (i == j)
					B[k][k] = tmp;
			}
}

/*
 * trans_64x64 - This optimized transpose function is for 64x64 matrices.
 */
#define A(r,c) (A[i+(r)][j+(c)])
#define B(r,c) (B[j+(r)][i+(c)])
void trans_64x64(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, k;
	int r0, r1, r2, r3, r4, r5, r6, r7;

	for (i = 0; i < 64; i += 8)
		for (j = 0; j < 64; j += 8) {
			for (k = 0; k < 4; k++) {
				r0 = A(k, 0);
				r1 = A(k, 1);
				r2 = A(k, 2);
				r3 = A(k, 3);
				r4 = A(k, 4);
				r5 = A(k, 5);
				r6 = A(k, 6);
				r7 = A(k, 7);

				B(0, k+0) = r0;
				B(1, k+0) = r1;
				B(2, k+0) = r2;
				B(3, k+0) = r3;
				B(0, k+4) = r7;
				B(1, k+4) = r6;
				B(2, k+4) = r5;
				B(3, k+4) = r4;
			}

			for (k = 0; k < 4; k++) {
				r0 = A(4, 3-k);
				r1 = A(5, 3-k);
				r2 = A(6, 3-k);
				r3 = A(7, 3-k);
				r4 = A(4, k+4);
				r5 = A(5, k+4);
				r6 = A(6, k+4);
				r7 = A(7, k+4);

				B(k+4, 0) = B(3-k, 4);
				B(k+4, 1) = B(3-k, 5);
				B(k+4, 2) = B(3-k, 6);
				B(k+4, 3) = B(3-k, 7);

				B(3-k, 4) = r0;
				B(3-k, 5) = r1;
				B(3-k, 6) = r2;
				B(3-k, 7) = r3;
				B(k+4, 4) = r4;
				B(k+4, 5) = r5;
				B(k+4, 6) = r6;
				B(k+4, 7) = r7;
			}
		}
}

