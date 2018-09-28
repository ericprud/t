import java.io.PrintStream;

/* Java program to solve N Queen Problem using
   backtracking */
public class NQueens
{
    int N = 8;
    boolean allowLines;

    NQueens(int size) {
        N = size;
        this.allowLines = false;
    }

    NQueens(int size, boolean allowLines) {
        N = size;
        this.allowLines = allowLines;
    }

    /* A utility function to print solution */
    void printSolution(int board[][], PrintStream out) {
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++)
                out.print(" " + (board[i][j] > 0 ? 'Q' : '.')
                                 + " ");
            out.println();
        }
    }

    /* A utility function to check if a queen can
       be placed on board[row][col]. Note that this
       function is called when "col" queens are already
       placeed in columns from 0 to col -1. So we need
       to check only left side for attacking queens */
    boolean isSafe(int board[][], int row, int col) {
        int i, j;

        /* Check this row on left side */
        for (i = 0; i < col; i++)
            if (board[row][i] == 1)
                return false;

        /* Check upper diagonal on left side */
        for (i=row, j=col; i>=0 && j>=0; i--, j--)
            if (board[i][j] == 1)
                return false;

        /* Check lower diagonal on left side */
        for (i=row, j=col; j>=0 && i<N; i++, j--)
            if (board[i][j] == 1)
                return false;

        return true;
    }

    /* Mark all the lines implied by earlier queens. */
    void stakeOutLines (int board[][], int N, int row, int col, int incr) {
        if (allowLines)
            return;

        for (int lookBackCol = 0; lookBackCol < col; lookBackCol++) {
            for (int lookBackRow = 0; lookBackRow < N; lookBackRow++) {
                if (board[lookBackRow][lookBackCol] > 0) {
                    int deltaCol = col - lookBackCol;
                    int deltaRow = row - lookBackRow;
                    int prohibitCol, prohibitRow;
                    for (prohibitCol = col + deltaCol, prohibitRow = row + deltaRow;
                         prohibitCol >= 0 && prohibitCol < N && prohibitRow >= 0 && prohibitRow < N;
                         prohibitCol += deltaCol, prohibitRow += deltaRow) {
                        board[prohibitRow][prohibitCol] += incr;
                    }
                    // should break to next column here.
                }
            }
        }
    }

    /* A recursive utility function to solve N
       Queen problem */
    boolean solveNQUtil(int board[][], int col) {
        /* base case: If all queens are placed
           then return true */
        if (col >= N)
            return true;

        /* Consider this column and try placing
           this queen in all rows one by one */
        for (int i = 0; i < N; i++) {
            /* Check if the queen can be placed on
               board[i][col] */
            if (board[i][col] == 0 && isSafe(board, i, col)) {
                /* Place this queen in board[i][col] */
                board[i][col] = 1;

                /* decrement all of the prohibited spots */
                stakeOutLines(board, N, i, col, -1);

                /* recur to place rest of the queens */
                boolean done = solveNQUtil(board, col + 1);

                /* restore the prohibited markers
                   (Can move to after return to see artifacts.)*/
                stakeOutLines(board, N, i, col, +1);

                if (done == true)
                    return true;

                /* If placing queen in board[i][col]
                   doesn't lead to a solution then
                   remove queen from board[i][col] */
                board[i][col] = 0; // BACKTRACK
            }
        }

        /* If the queen can not be placed in any row in
           this colum col, then return false */
        return false;
    }

    /* This function solves the N Queen problem using
       Backtracking. It mainly uses solveNQUtil () to
       solve the problem. It returns false if queens
       cannot be placed, otherwise, return true and
       prints placement of queens in the form of 1s.
       Please note that there may be more than one
       solutions, this function prints one of the
       feasible solutions.*/
    int[][] solveNQ() {
        int[][] board = new int[N][N];
        return solveNQUtil(board, 0) ? board : null;
    }

    // driver program to test above function
    public static void main(String args[]) {
        int n;

        if(args.length > 0) {
            try {
                n = Integer.parseInt(args[0]);
            } catch (NumberFormatException e) {
                System.err.println("Expected small-ish integer, not \"" + args[0] + "\".");
                System.exit(1);
                n = 0; // keep java's SSA from whining.
            }
        } else {
            final int DEFAULT_SIZE = 8;
            System.err.println("Usage: NQueens <int>\nusing " + DEFAULT_SIZE);
            n = DEFAULT_SIZE;
        }
        NQueens queenz = new NQueens(n);
        int[][] board = queenz.solveNQ();

        if (board == null) {
            System.err.println("Solution does not exist for " + n + "x" + n + " board.");
        } else {
            System.out.println(n + ":");
            queenz.printSolution(board, System.out);
        }
    }
}
