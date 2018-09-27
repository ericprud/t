/* Test the NQueens "library".
 * ISSUES:
 *   1. These test a predicted solution, not every allowable solution.
 *   2. The @Tests could probably be iteratively generated from an array
 *      of parameters.
 */
import org.junit.Test;
import static org.junit.Assert.*;

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class NQueensTest {
    static final String UTF8 = "UTF-8";
    static final int[][] Soln1 = {{1}};
    static final String Soln1Str = " Q \n";
    static final int[][] Soln4 =
        {{0, 0, 1, 0},
         {1, 0, 0, 0},
         {0, 0, 0, 1},
         {0, 1, 0, 0}};
    static final String Soln4Str =
        " .  .  Q  . \n" +
        " Q  .  .  . \n" +
        " .  .  .  Q \n" +
        " .  Q  .  . \n";
    static final int[][] Soln8 = {
        {0, 0, 0, 0, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0, 0, 0, 0}};
    static final String Soln8Str =
        " .  .  .  .  Q  .  .  . \n" +
        " .  .  .  .  .  .  Q  . \n" +
        " Q  .  .  .  .  .  .  . \n" +
        " .  .  .  Q  .  .  .  . \n" +
        " .  Q  .  .  .  .  .  . \n" +
        " .  .  .  .  .  .  .  Q \n" +
        " .  .  .  .  .  Q  .  . \n" +
        " .  .  Q  .  .  .  .  . \n";

    public static boolean Board (int n, int[][] ref) {
        final ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            PrintStream ps = new PrintStream(baos, true, UTF8);
            NQueens queenz = new NQueens(n); 
            int[][] b = queenz.solveNQ();
            return Arrays.deepEquals(b, ref);
        } catch (UnsupportedEncodingException e) {
            throw new AssertionError("harness failure -- " + UTF8 + " encoding unknown");
        }
    }

    public static boolean Print (int n, int[][] soln, String ref) {
        final ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            PrintStream ps = new PrintStream(baos, true, UTF8);
            NQueens queenz = new NQueens(n); 
            queenz.printSolution(soln, ps);
            return ref.equals("" + baos);
        } catch (UnsupportedEncodingException e) {
            throw new AssertionError("harness failure -- " + UTF8 + " encoding unknown");
        }
    }

    @Test public void test1() { assertTrue("1 should pass", Board(1, Soln1)); }
    @Test public void test2() { assertTrue("2 should fail", Board(2, null)); }
    @Test public void test3() { assertTrue("3 should fail", Board(3, null)); }
    @Test public void test4() { assertTrue("4 should pass", Board(4, Soln4)); }
    @Test public void test5() { assertTrue("5 should fail", Board(5, null)); }
    @Test public void test6() { assertTrue("6 should fail", Board(6, null)); }
    @Test public void test7() { assertTrue("7 should fail", Board(7, null)); }
    @Test public void test8() { assertTrue("8 should pass", Board(8, Soln8)); }

    @Test public void prnt1() { assertTrue("1 should print", Print(1, Soln1, Soln1Str)); }
    @Test public void prnt4() { assertTrue("4 should print", Print(4, Soln4, Soln4Str)); }
    @Test public void prnt8() { assertTrue("8 should print", Print(8, Soln8, Soln8Str)); }

    @Test public void integration() {
        final ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            PrintStream ps = new PrintStream(baos, true, UTF8);
            final int n = 1;
            NQueens queenz = new NQueens(n); 
            int[][] board = queenz.solveNQ();

        } catch (UnsupportedEncodingException e) {
            throw new AssertionError("harness failure -- " + UTF8 + " encoding unknown");
        }
    }
}
