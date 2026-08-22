#include<iostream>
#include<vector>
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

// worker process :
// in worker process we take some pairs of columns of A and rows of B and then find their corresponding partial matrix C and returns it to the master
vector<long long> worker_process(int m, int p, int num_pairs, const vector<long long>& column_A, const vector<long long>& row_B){
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

    return partial_C;
}

// currently my main function is the master process/ node as of now, will need to make it rank 0 for the MPI part
int main(int argc, char** argv){
    int m, n, p;

    cout << "enter rows of matrix A : " << endl;
    cin >> m;

    cout << "enter cols of matrix A == rows of matrix  B : " << endl;
    cin >> n;

    cout << "enter cols of matrix B : " << endl;
    cin >> p;

    // why are we storing in 1d array and not in the usual 2d array ??
    // because the mpi commands responsible for sending data need contiguous chunks, but the 2d vector does not store all the rows contiguously
    vector<long long> matrixA(m * n);
    vector<long long> matrixB(n * p);

    // storing matrix in column major, because we need to send the columns of A in contiguous manner
    cout << "enter elements of matrix A : " << endl;
    for(int i = 0; i < m; i++){
        for(int j = 0; j < n; j++){
            long long x; cin >> x;

            // to find column major index we do, current column *
            int col_maj_index = (j * m) + i; // j * m gives us the 0th element address of the jth column then we add i to get address of the ith element
            matrixA[col_maj_index] = x;
        }
    }

    // storing the elements of B in row major order
    cout << "enter elements of matrix B : " << endl;
    for(int i = 0; i < n * p; i++){
        long long x; cin >> x;
        matrixB[i] = x;
    }

    cout << "Enter the number of processes to simulate : " << endl;
    int P; // number of proceses
    cin >> P;

    int remainder = n % P;
    int min_pairs_to_all = n / P; // each distributed process will have atleast min_pairs_to_all pairs

    vector<int> to_send(P, min_pairs_to_all);

    for(int i = 0; i < to_send.size() && remainder > 0; i++){
        to_send[i]++; // assigning the remaining pairs that we are left with
        remainder--;
    }

    vector<int> starting_point_A(P); vector<int> chunk_size_A(P);
    vector<int> starting_point_B(P); vector<int> chunk_size_B(P);

    for(int i = 0; i < P; i++){
        chunk_size_A[i] = to_send[i] * m; // 1 column of A has m elements
        chunk_size_B[i] = to_send[i] * p; // 1 row of B has p elements

        if(i == 0){
            starting_point_A[i] = 0;
            starting_point_B[i] = 0;
        }else{
            starting_point_A[i] = starting_point_A[i - 1] + chunk_size_A[i - 1];
            starting_point_B[i] = starting_point_B[i - 1] + chunk_size_B[i - 1];
        }
    }

    vector<long long> matrix_C(m * p, 0);
    for(int i = 0; i < to_send.size(); i++){
        cout << "Starting with distributed process rank : " << i << " pairs received : " << to_send[i] << endl;
        int to_send_current = to_send[i];
        
        // now i have to assign column a and row b
        // what info do i have : starting point of each pair, chunk size for that pair
        // mistake i had done here : had accidentaly added 1 to the end iterator, but there is no need, cause if start index = 0 and the chunk size is 6, then it will include [0, 5] so no need of that extra 1
        vector<long long> column_A(matrixA.begin() + starting_point_A[i], matrixA.begin() + starting_point_A[i] + chunk_size_A[i]);
        vector<long long> row_B(matrixB.begin() + starting_point_B[i], matrixB.begin() + starting_point_B[i] + chunk_size_B[i]);

        vector<long long> partial_C = worker_process(m, p, to_send_current, column_A, row_B);

        for(int k = 0; k < partial_C.size(); k++){
            matrix_C[k] += partial_C[k];
        }

        cout << "received partial matrix from distributed process with rank : " << i << endl;
    }

    cout << "the final product matrix C is : " << endl;
    printSquareMatrix(matrix_C, m, p);
    return 0;
}
