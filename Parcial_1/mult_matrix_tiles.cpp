#include <stdio.h>
#include <cuda.h>
#include <time.h>
#include <iostream>
#define H 1000
#define W 1000
#define TILE_WIDTH 32

using namespace std;

__global__ void matrixMulKernelTiled(int *d_M, int *d_N, int *d_P, int width) {
  __shared__ int Mds[TILE_WIDTH][TILE_WIDTH];
  __shared__ int Nds[TILE_WIDTH][TILE_WIDTH];

  int bx = blockIdx.x;
  int by = blockIdx.y;
  int tx = threadIdx.x;
  int ty = threadIdx.y;

  int row = by * TILE_WIDTH + ty;
  int col = bx * TILE_WIDTH + tx;

  float Pvalue = 0;

  for (int m = 0; m < width / TILE_WIDTH; ++m) {
    Mds[ty][tx] = d_M[row * width + m * TILE_WIDTH + tx];
    Nds[ty][tx] = d_N[(m * TILE_WIDTH + ty) * width + col];
    __syncthreads();

    for (int k = 0; k < TILE_WIDTH; ++k) {
      Pvalue += Mds[ty][k] * Nds[k][tx];
    }
    __syncthreads();
  }
  d_P[row * width + col] = Pvalue;
}

void fill(int *v) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      v[i * W + j] = 2;
    }
  }
}

void mult(int *A, int *B, int *C) {
  int aux = 0;
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      aux = 0;
      for (int k = 0; k < H; k++) aux += A[i * W + k] * B[k * W + j];
      C[i * W + j] = aux;
    }
  }
}

void mostrar(int *v) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      cout << v[i * W + j] << " ";
    }
    cout << endl;
  }
}

__global__ void multMat(int *d_A, int *d_B, int *d_C) {
  int i = blockIdx.y * blockDim.y + threadIdx.y;
  int j = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < H && j < W) {
    int Pvalue = 0;
    for (int k = 0; k < H; k++) {
      Pvalue += d_A[i * W + k] * d_B[k * W + j];
    }
    d_C[i * W + j] = Pvalue;
  }
}

int main() {
  clock_t start, end;
  double tiempo;
  int *A = (int *)malloc(H * W * sizeof(int));
  int *B = (int *)malloc(H * W * sizeof(int));
  int *C = (int *)malloc(H * W * sizeof(int));
  int *D = (int *)malloc(H * W * sizeof(int));
  int width = W;
  fill(A);
  fill(B);
  start = clock();
  mult(A, B, C);

  end = clock();
  tiempo = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Tiempo CPU = %lf s\n", tiempo);

  ///------------------ Paraleo-----------------------//////
  int *d_A, *d_B, *d_D;
  float blockSize = 32;
  dim3 dimBlock(blockSize, blockSize);
  dim3 dimGrid(ceil(W / float(blockSize)), ceil(H / float(blockSize)), 1);

  cudaMalloc((void **)&d_A, sizeof(int) * H * W);
  cudaMalloc((void **)&d_B, sizeof(int) * H * W);
  cudaMalloc((void **)&d_D, sizeof(int) * H * W);

  start = clock();

  cudaMemcpy(d_A, A, sizeof(int) * H * W, cudaMemcpyHostToDevice);
  cudaMemcpy(d_B, B, sizeof(int) * H * W, cudaMemcpyHostToDevice);

  // multMat<<<dimGrid, dimBlock>>>(d_A, d_B, d_D);

  matrixMulKernelTiled<<<dimGrid, dimBlock>>>(d_A, d_B, d_D, width);

  cudaMemcpy(D, d_D, sizeof(int) * H * W, cudaMemcpyDeviceToHost);
  end = clock();

  tiempo = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Tiempo GPU = %lf s\n", tiempo);

  // mostrar(D);
  free(A);
  free(B);
  free(C);
  free(D);

  cudaFree(d_A);
  cudaFree(d_B);
  cudaFree(d_D);
}