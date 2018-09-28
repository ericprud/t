/*
 * Compare NQueens runs with and without the lines constraint.
 *
 * The lines constraint may or may not change the solution and the
 * search time.
 */

import java.util.Arrays;

public class Compare {
    public static void main(String args[]) {
        for (int n = 1; ; ++n)  {
            long a = java.lang.System.currentTimeMillis();
            int[][] allow = new NQueens(n, true).solveNQ();
            long b = java.lang.System.currentTimeMillis();
            int[][] deny = new NQueens(n, false).solveNQ();
            long c = java.lang.System.currentTimeMillis();
            System.out.println(n + " " + Arrays.deepEquals(allow, deny) + " " +
                               (double)(c-b)/(b-a));
        }
    }
}
