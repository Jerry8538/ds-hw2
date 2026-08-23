#include<iostream>
#include<vector>
#include<fstream>
#include<mpi.h>
using namespace std;

// helper functions

// prints matrix in 2d form
void printSquareMatrix(const vector<long long>& matrix, int rows, int cols){
    int row_count = 0;
    while(row_count < rows){
        int col_count = 0;
        while(col_count < cols){
            cout << matrix[row_count * cols + col_count] << " ";
            col_count++;
        }
        cout << endl;
        row_count++;
    }
}

void master_process(int m, int n, int p, int P, ifstream &inpFile) {
    // why are we storing in 1d array and not in the usual 2d array ??
    // because the mpi commands responsible for sending data need contiguous chunks, but the 2d vector does not store all the rows contiguously
    vector<long long> matrixA(m * n);
    vector<long long> matrixB(n * p);

    // storing matrix in column major, because we need to send the columns of A in contiguous manner
    for(int i = 0; i < m; i++){
        for(int j = 0; j < n; j++){
            long long x; inpFile >> x;

            // to find column major index we do, current column *
            int col_maj_index = (j * m) + i; // j * m gives us the 0th element address of the jth column then we add i to get address of the ith element
            matrixA[col_maj_index] = x;
        }
    }

    // storing the elements of B in row major order
    for(int i = 0; i < n * p; i++){
        inpFile >> matrixB[i];
    }

    int remainder = n % P;
    int min_pairs_to_all = n / P; // each distributed process will have atleast min_pairs_to_all pairs

    vector<int> to_send(P+1, min_pairs_to_all);
    to_send[0] = 0;

    for(int i = 1; i < to_send.size() && remainder > 0; i++){
        to_send[i]++; // assigning the remaining pairs that we are left with
        remainder--;
    }

    // send each process its num_pairs
    MPI_Scatter(to_send.data(), 1, MPI_INT, MPI_IN_PLACE, 0, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> starting_point_A(P+1); vector<int> chunk_size_A(P+1);
    vector<int> starting_point_B(P+1); vector<int> chunk_size_B(P+1);

    for(int i = 0; i <= P; i++){
        chunk_size_A[i] = to_send[i] * m; // 1 column of A has m elements
        chunk_size_B[i] = to_send[i] * p; // 1 row of B has p elements

        starting_point_A[i] = starting_point_A[i - 1] + chunk_size_A[i - 1];
        starting_point_B[i] = starting_point_B[i - 1] + chunk_size_B[i - 1];
    }

    // send each process its columns of A
    MPI_Scatterv(matrixA.data(), chunk_size_A.data(), starting_point_A.data(), MPI_LONG_LONG,
                 MPI_IN_PLACE, 0, MPI_LONG_LONG,
                 0, MPI_COMM_WORLD);

    // send each process its rows of B
    MPI_Scatterv(matrixB.data(), chunk_size_B.data(), starting_point_B.data(), MPI_LONG_LONG,
            MPI_IN_PLACE, 0, MPI_LONG_LONG,
            0, MPI_COMM_WORLD);

    vector<long long> matrix_C(m * p, 0);
    // NOTE: if array is passed to MPI_Reduce, it applies op to each element
    MPI_Reduce(MPI_IN_PLACE, matrix_C.data(), m*p, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    cout << "the final product matrix C:\n";
    printSquareMatrix(matrix_C, m, p);
}

// worker process :
// in worker process we take some pairs of columns of A and rows of B and then find their corresponding partial matrix C and returns it to the master
void worker_process(int m, int p){
    // receiving data from master
    int num_pairs;
    MPI_Scatter(NULL, 0, MPI_INT, &num_pairs, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cout << "my num_pairs: " << num_pairs << endl;

    vector<long long> column_A(m*num_pairs);
    MPI_Scatterv(NULL, NULL, NULL, MPI_LONG_LONG,
                 column_A.data(), m*num_pairs, MPI_LONG_LONG,
                 0, MPI_COMM_WORLD);

    for (auto i : column_A) cout << i << ' ';
    cout << endl;

    vector<long long> row_B(p*num_pairs);
    MPI_Scatterv(NULL, NULL, NULL, MPI_LONG_LONG,
                 row_B.data(), p*num_pairs, MPI_LONG_LONG,
                 0, MPI_COMM_WORLD);

    for (auto i : row_B) cout << i << ' ';
    cout << endl;

    // here m is the rows of A, p is the columns of B and so the partial matrix C will be m x p
    // int num_pairs is the number of pairs assigned to this worker process
    // column_A is/are the columns that we need to process for this particular worker process and row_B is/are the rows that we need for this worker process

    vector<long long> partial_C(m*p, 0);
    int pairs_processed = 0;
    // pairs_processes also gives us the current column and row that we are processing
    while(pairs_processed < num_pairs){
        // pairs processed gives us the current column and row that we are looking at
        for(int i = 0; i < m; i++){
            // traversing the column elements of the current column
            for(int j = 0; j < p; j++){
                int a_ind = pairs_processed * m + i; // why pairs_processed * m + i ?? because pairs_processed * m gives us the 0th element of the current column of A and then we add i to get the current column element that we are looking at
                int b_ind = pairs_processed * p + j; // same logic as above

                partial_C[i * p + j] += column_A[a_ind] * row_B[b_ind]; // i * p + j, why ?? because this gives us the index in row major form
            }
        }
        pairs_processed++;
    }
    cout << "my partial matrix:\n";
    printSquareMatrix(partial_C, m, p);

    // send partial matrix to be reduced in master
    MPI_Reduce(partial_C.data(), NULL, m*p, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
}

int main(int argc, char** argv){
    MPI_Init(NULL, NULL);
    int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int m, n, p;
    ifstream inpFile;
    if (rank == 0) {
        inpFile.open("in");
        inpFile >> m >> n >> p;
    }
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&p, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cout << "rank: " << rank << endl << m << ' ' << n << ' ' << p << endl;

    if (rank == 0) {
        int size; MPI_Comm_size(MPI_COMM_WORLD, &size);
        master_process(m, n, p, size-1, inpFile);
    } else {
        worker_process(m, p);
    }
    MPI_Finalize();
}
