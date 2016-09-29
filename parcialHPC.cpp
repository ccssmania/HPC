#include <cmath>
#include <ctime>
#include <iostream>
#include <vector>

#include <cuda.h>




int height_A = 5 ;
int width_A = 7 ;
int height_B = 7;
int width_B = 3;
__global__ void multKernelTiled(const float* d_M, const float* d_N, float* d_R,
                                int width_M, int height_M, int width_N,
                                int tileWidth) {
    extern __shared__ float buffer[];

    // declaración dl tamaño de los bloques de memoria compartida
    float* ds_M = &buffer[0];
    float* ds_N = &buffer[tileWidth * tileWidth];

    // Referencia a la ID del bloque y del hilo
    int bx = blockIdx.x;
    int by = blockIdx.y;
    int tx = threadIdx.x;
    int ty = threadIdx.y;

    // dimension del Tile
    int row = by * tileWidth + ty;
    int col = bx * tileWidth + tx;

    // Variable para almacenar el valor final de la multiplicación de la
    // posición
    // [ty, tx]
    float Pvalue = 0;

    // Se recorre cada uno de los Tiles
    for (int p = 0; p < (tileWidth + width_M - 1) / tileWidth; p++) {
        // for (int p = 0; p < width_M / tileWidth; p++) {
        // Esta asigne los valores correspondientes de cada fila a la memoria compartida
        if (row < height_M && (p * tileWidth + tx) < width_M) {
            ds_M[ty * tileWidth + tx] = d_M[row * width_M + p * tileWidth + tx];
        } else {
            ds_M[ty * tileWidth + tx] = 0.0f;
        }

        //se asigna valores correspondientes en cada posición, pero si se sale del rango se asigna
        if ((p * tileWidth + ty) < width_M && col < width_N) {
            ds_N[ty * tileWidth + tx] =
                d_N[(p * tileWidth + ty) * width_N + col];
        } else {
            ds_N[ty * tileWidth + tx] = 0.0f;
        }

        __syncthreads(); 
        //sincroniza y espera los resultados de cada hilo

        // Se realiza la multiplicación
        if (row < height_M && col < width_N) {
            for (int k = 0; k < tileWidth; k++) {
                Pvalue += ds_M[ty * tileWidth + k] * ds_N[k * tileWidth + tx];
            }
        }

        __syncthreads(); 
        //sincroniza y espera los resultados de cada hilo

    }

  //se asigna los resultados a la matriz respuesta
    if (row < height_M && col < width_N) {
        d_R[row * width_N + col] = Pvalue;
    }
}

void mult(const float* A, const float* B, float* C, int width_A, int height_A,
          int width_B) {
    int aux = 0;
    for (int i = 0; i < height_A; i++) {
        for (int j = 0; j < width_B; j++) {
            aux = 0;
            for (int k = 0; k < width_A; k++)
                aux += A[i * width_A + k] * B[k * width_B + j];
            C[i * width_B + j] = aux;
        }
    }
}

void fillMatrix(float* m, int width, int height) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            m[i * width + j] = 2.0;
        }
    }
}


void showMatrix(const float* m, int width, int height) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (j) std::cout << " ";
            std::cout << m[i * width + j];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}



int main() {
	float* A = new float[height_A * width_A];
    float* B = new float[width_A * width_B];
    float* C = new float[width_A * width_B];
    float* D = new float[width_A * width_B];

    int tileWidth = 1;
    
    size_t memSize = (2 * tileWidth * tileWidth) * sizeof(float);

    fillMatrix(A, width_A, height_A);
    fillMatrix(B, width_B, width_A);

    float *d_A, *d_B, *d_D;
    int blocksize = tileWidth;

    dim3 dimBlock(blocksize, blocksize, 1);
    dim3 dimGrid(ceil(width_B / float(blocksize)),
                 ceil(height_A / float(blocksize)), 1);

    cudaMalloc(&d_A, sizeof(float) * height_A * width_A);
    cudaMalloc(&d_B, sizeof(float) * width_A * width_B);
    cudaMalloc(&d_D, sizeof(float) * height_A * width_B);


    // Multiplicacion secuencial CPU
   	double cpu_time = 0;
    clock_t startCPU = clock();
    mult(A, B, C, width_A, height_A, width_B);
    clock_t end = clock();
    cpu_time = double(end - startCPU) / CLOCKS_PER_SEC;
    std::cout << "Tiempo invertido CPU = " << cpu_time << "s\n";
   
    

    // Multiplicacion paralela GPU with tiles
    double gpu_time = 0;
    clock_t startGPU = clock();

    cudaMemcpy(d_A, A, sizeof(float) * height_A * width_A,
               cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, B, sizeof(float) * width_A * width_B,
               cudaMemcpyHostToDevice);

    multKernelTiled<<<dimGrid, dimBlock, memSize>>>(
        d_A, d_B, d_D, width_A, height_A, width_B, tileWidth);
    cudaMemcpy(D, d_D, sizeof(float) * height_A * width_B,
               cudaMemcpyDeviceToHost);

    clock_t endGPU = clock();
    gpu_time = double(endGPU - startGPU) / CLOCKS_PER_SEC;
    std::cout << "Tiempo invertido GPU = " << gpu_time << "s\n";
  

    std::cout << "La aceleracion total obtenida es de: "
              << (cpu_time / gpu_time) << "x" << std::endl;
  
   	//showMatrix(C, width_B,height_A);
   	//showMatrix(D,width_B,height_A);
    delete A;
    delete B;
    delete C;
    delete D;

    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_D);

    return 0;
   
}