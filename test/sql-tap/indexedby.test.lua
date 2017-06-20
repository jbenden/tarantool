#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(30)

--!./tcltestrunner.lua
-- 2008-10-04
--
-- The author disclaims copyright to this source code.  In place of
-- a legal notice, here is a blessing:
--
--    May you do good and not evil.
--    May you find forgiveness for yourself and forgive others.
--    May you share freely, never taking more than you give.
--
-------------------------------------------------------------------------
--
-- ["set","testdir",[["file","dirname",["argv0"]]]]
-- ["source",[["testdir"],"\/tester.tcl"]]
testprefix = "indexedby"
-- Create a schema with some indexes.
--
test:do_execsql_test(
    "indexedby-1.1",
    [[
        CREATE TABLE t1(id primary key, a, b);
        CREATE INDEX i1 ON t1(a);
        CREATE INDEX i2 ON t1(b);

        CREATE TABLE t2(id primary key, c, d);
        CREATE INDEX i3 ON t2(c);
        CREATE INDEX i4 ON t2(d);

        CREATE TABLE t3(e PRIMARY KEY, f);

        CREATE VIEW v1 AS SELECT * FROM t1;
    ]], {
        -- <indexedby-1.1>
        
        -- </indexedby-1.1>
    })

-- Explain Query Plan
--
local function EQP(sql)
    X(41, "X!cmd", [=[["uplevel",["execsql {EXPLAIN QUERY PLAN ",["sql"],"}"]]]=])
end

-- These tests are to check that "EXPLAIN QUERY PLAN" is working as expected.
--
test:do_execsql_test(
    "indexedby-1.2",
    [[
        EXPLAIN QUERY PLAN select * from t1 WHERE a = 10; 
    ]], {
        -- <indexedby-1.2>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a=?)"
        -- </indexedby-1.2>
    })

test:do_execsql_test(
    "indexedby-1.3",
    [[
        EXPLAIN QUERY PLAN select * from t1 ; 
    ]], {
        -- <indexedby-1.3>
        0, 0, 0, "SCAN TABLE t1"
        -- </indexedby-1.3>
    })

test:do_execsql_test(
    "indexedby-1.4",
    [[
        EXPLAIN QUERY PLAN select * from t1, t2 WHERE c = 10; 
    ]], {
        -- <indexedby-1.4>
        0, 0, 1, "SEARCH TABLE t2 USING INDEX i3 (c=?)", 0, 1, 0, "SCAN TABLE t1"
        -- </indexedby-1.4>
    })

-- Parser tests. Test that an INDEXED BY or NOT INDEX clause can be 
-- attached to a table in the FROM clause, but not to a sub-select or
-- SQL view. Also test that specifying an index that does not exist or
-- is attached to a different table is detected as an error.
--
-- EVIDENCE-OF: R-07004-11522 -- syntax diagram qualified-table-name
-- 
-- EVIDENCE-OF: R-58230-57098 The "INDEXED BY index-name" phrase
-- specifies that the named index must be used in order to look up values
-- on the preceding table.
--
-- do_test indexedby-2.1 {
--   execsql { SELECT * FROM t1 NOT INDEXED WHERE a = 'one' AND b = 'two'}
-- } {}
-- do_test indexedby-2.1b {
--   execsql { SELECT * FROM main.t1 NOT INDEXED WHERE a = 'one' AND b = 'two'}
-- } {}
test:do_execsql_test(
    "indexedby-2.2",
    [[
        SELECT * FROM t1 INDEXED BY 'i1' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.2>
        
        -- </indexedby-2.2>
    })

test:do_execsql_test(
    "indexedby-2.2b",
    [[
        SELECT * FROM main.t1 INDEXED BY 'i1' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.2b>
        
        -- </indexedby-2.2b>
    })

test:do_execsql_test(
    "indexedby-2.3",
    [[
        SELECT * FROM t1 INDEXED BY 'i2' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.3>
        
        -- </indexedby-2.3>
    })

-- EVIDENCE-OF: R-44699-55558 The INDEXED BY clause does not give the
-- optimizer hints about which index to use; it gives the optimizer a
-- requirement of which index to use.
-- EVIDENCE-OF: R-15800-25719 If index-name does not exist or cannot be
-- used for the query, then the preparation of the SQL statement fails.
--
test:do_catchsql_test(
    "indexedby-2.4",
    [[
        SELECT * FROM t1 INDEXED BY 'i3' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.4>
        1, "no such index: i3"
        -- </indexedby-2.4>
    })

-- EVIDENCE-OF: R-62112-42456 If the query optimizer is unable to use the
-- index specified by the INDEX BY clause, then the query will fail with
-- an error.
-- do_test indexedby-2.4.1 {
--   catchsql { SELECT b FROM t1 INDEXED BY 'i1' WHERE b = 'two' }
-- } {1 {no query solution}}
test:do_catchsql_test(
    "indexedby-2.5",
    [[
        SELECT * FROM t1 INDEXED BY i5 WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.5>
        1, "no such index: i5"
        -- </indexedby-2.5>
    })

test:do_catchsql_test(
    "indexedby-2.6",
    [[
        SELECT * FROM t1 INDEXED BY WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-2.6>
        1, [[near "WHERE": syntax error]]
        -- </indexedby-2.6>
    })

test:do_catchsql_test(
    "indexedby-2.7",
    [[
        SELECT * FROM v1 INDEXED BY 'i1' WHERE a = 'one' 
    ]], {
        -- <indexedby-2.7>
        1, "no such index: i1"
        -- </indexedby-2.7>
    })

-- Tests for single table cases.
--
-- EVIDENCE-OF: R-37002-28871 The "NOT INDEXED" clause specifies that no
-- index shall be used when accessing the preceding table, including
-- implied indices create by UNIQUE and PRIMARY KEY constraints. However,
-- the rowid can still be used to look up entries even when "NOT INDEXED"
-- is specified.
--
-- do_execsql_test indexedby-3.1 {
--   EXPLAIN QUERY PLAN SELECT * FROM t1 WHERE a = 'one' AND b = 'two'
-- } {/SEARCH TABLE t1 USING INDEX/}
-- do_execsql_test indexedby-3.1.1 {
--   EXPLAIN QUERY PLAN SELECT * FROM t1 NOT INDEXED WHERE a = 'one' AND b = 'two'
-- } {0 0 0 {SCAN TABLE t1}}
-- do_execsql_test indexedby-3.1.2 {
--   EXPLAIN QUERY PLAN SELECT * FROM t1 NOT INDEXED WHERE rowid=1
-- } {/SEARCH TABLE t1 USING INTEGER PRIMARY KEY .rowid=/}
test:do_execsql_test(
    "indexedby-3.2",
    [[
        EXPLAIN QUERY PLAN 
        SELECT * FROM t1 INDEXED BY 'i1' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-3.2>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a=?)"
        -- </indexedby-3.2>
    })

test:do_execsql_test(
    "indexedby-3.3",
    [[
        EXPLAIN QUERY PLAN 
        SELECT * FROM t1 INDEXED BY 'i2' WHERE a = 'one' AND b = 'two'
    ]], {
        -- <indexedby-3.3>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i2 (b=?)"
        -- </indexedby-3.3>
    })

-- do_test indexedby-3.4 {
--   catchsql { SELECT * FROM t1 INDEXED BY 'i2' WHERE a = 'one' }
-- } {1 {no query solution}}
-- do_test indexedby-3.5 {
--   catchsql { SELECT * FROM t1 INDEXED BY 'i2' ORDER BY a }
-- } {1 {no query solution}}
test:do_catchsql_test(
    "indexedby-3.6",
    [[
        SELECT * FROM t1 INDEXED BY 'i1' WHERE a = 'one' 
    ]], {
        -- <indexedby-3.6>
        0, {}
        -- </indexedby-3.6>
    })

test:do_catchsql_test(
    "indexedby-3.7",
    [[
        SELECT * FROM t1 INDEXED BY 'i1' ORDER BY a 
    ]], {
        -- <indexedby-3.7>
        0, {}
        -- </indexedby-3.7>
    })

-- do_execsql_test indexedby-3.8 {
--   EXPLAIN QUERY PLAN 
--   SELECT * FROM t3 INDEXED BY sqlite_autoindex_t3_1 ORDER BY e 
-- } {0 0 0 {SCAN TABLE t3 USING INDEX sqlite_autoindex_t3_1}}
-- do_execsql_test indexedby-3.9 {
--   EXPLAIN QUERY PLAN 
--   SELECT * FROM t3 INDEXED BY sqlite_autoindex_t3_1 WHERE e = 10 
-- } {0 0 0 {SEARCH TABLE t3 USING INDEX sqlite_autoindex_t3_1 (e=?)}}
-- do_test indexedby-3.10 {
--   catchsql { SELECT * FROM t3 INDEXED BY sqlite_autoindex_t3_1 WHERE f = 10 }
-- } {1 {no query solution}}
-- do_test indexedby-3.11 {
--   catchsql { SELECT * FROM t3 INDEXED BY sqlite_autoindex_t3_2 WHERE f = 10 }
-- } {1 {no such index: sqlite_autoindex_t3_2}}
-- Tests for multiple table cases.
--
test:do_execsql_test(
    "indexedby-4.1",
    [[
        EXPLAIN QUERY PLAN SELECT * FROM t1, t2 WHERE a = c 
    ]], {
        -- <indexedby-4.1>
        0, 0, 0, "SCAN TABLE t1 USING INDEX i2", 0, 1, 1, "SEARCH TABLE t2 USING INDEX i3 (c=?)"
        -- </indexedby-4.1>
    })

test:do_execsql_test(
    "indexedby-4.2",
    [[
        EXPLAIN QUERY PLAN SELECT * FROM t1 INDEXED BY 'i1', t2 WHERE a = c
    ]], {
        -- <indexedby-4.2>
        0, 0, 0, "SCAN TABLE t1 USING INDEX i1", 0, 1, 1, "SEARCH TABLE t2 USING INDEX i3 (c=?)"
        -- </indexedby-4.2>
    })

-- do_test indexedby-4.3 {
--   catchsql {
--     SELECT * FROM t1 INDEXED BY 'i1', t2 INDEXED BY 'i3' WHERE a=c
--   }
-- } {1 {no query solution}}
-- do_test indexedby-4.4 {
--   catchsql {
--     SELECT * FROM t2 INDEXED BY 'i3', t1 INDEXED BY 'i1' WHERE a=c
--   }
-- } {1 {no query solution}}
-- Test embedding an INDEXED BY in a CREATE VIEW statement. This block
-- also tests that nothing bad happens if an index refered to by
-- a CREATE VIEW statement is dropped and recreated.
--
test:do_execsql_test(
    "indexedby-5.1",
    [[
        CREATE VIEW v2 AS SELECT * FROM t1 INDEXED BY 'i1' WHERE a > 5;
        EXPLAIN QUERY PLAN SELECT * FROM v2 
    ]], {
        -- <indexedby-5.1>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a>?)"
        -- </indexedby-5.1>
    })

test:do_execsql_test(
    "indexedby-5.2",
    [[
        EXPLAIN QUERY PLAN SELECT * FROM v2 WHERE b = 10 
    ]], {
        -- <indexedby-5.2>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a>?)"
        -- </indexedby-5.2>
    })

test:do_test(
    "indexedby-5.3",
    function()
        test:execsql " DROP INDEX 'i1' "
        return test:catchsql " SELECT * FROM v2 "
    end, {
        -- <indexedby-5.3>
        1, "no such index: i1"
        -- </indexedby-5.3>
    })

-- MUST_WORK_TEST possibly related to indexing extra fields #2289 #2495
if 0>0 then
    -- Tarantool: Working in WITHOUT ROWID case.
test:do_test(
    "indexedby-5.4",
    function()
        -- Recreate index i1 in such a way as it cannot be used by the view query.
        test:execsql " CREATE INDEX i1 ON t1(b) "
        return test:catchsql " SELECT * FROM v2 "
    end, {
        -- <indexedby-5.4>
        1, "no query solution"
        -- </indexedby-5.4>
    })
else
    test:execsql " CREATE INDEX i1 ON t1(b) "
end

test:do_test(
    "indexedby-5.5",
    function()
        -- Drop and recreate index i1 again. This time, create it so that it can
        -- be used by the query.
        test:execsql " DROP INDEX 'i1' ; CREATE INDEX i1 ON t1(a) "
        return test:catchsql " SELECT * FROM v2 "
    end, {
        -- <indexedby-5.5>
        0, {}
        -- </indexedby-5.5>
    })

-- # Test that "NOT INDEXED" may use the rowid index, but not others.
-- # 
-- do_execsql_test indexedby-6.1 {
--   EXPLAIN QUERY PLAN SELECT * FROM t1 WHERE b = 10 ORDER BY rowid 
-- } {0 0 0 {SEARCH TABLE t1 USING INDEX i2 (b=?)}}
-- do_execsql_test indexedby-6.2 {
--   EXPLAIN QUERY PLAN SELECT * FROM t1 NOT INDEXED WHERE b = 10 ORDER BY rowid 
-- } {0 0 0 {SCAN TABLE t1}}
-- EVIDENCE-OF: R-40297-14464 The INDEXED BY phrase forces the SQLite
-- query planner to use a particular named index on a DELETE, SELECT, or
-- UPDATE statement.
--
-- Test that "INDEXED BY" can be used in a DELETE statement.
-- 
test:do_execsql_test(
    "indexedby-7.1",
    [[
        EXPLAIN QUERY PLAN DELETE FROM t1 WHERE a = 5 
    ]], {
        -- <indexedby-7.1>
        0, 0, 0, "SEARCH TABLE t1 USING COVERING INDEX i1 (a=?)"
        -- </indexedby-7.1>
    })

test:do_execsql_test(
    "indexedby-7.2",
    [[
        EXPLAIN QUERY PLAN DELETE FROM t1 NOT INDEXED WHERE a = 5 
    ]], {
        -- <indexedby-7.2>
        0, 0, 0, "SEARCH TABLE t1 USING COVERING INDEX i1 (a=?)"
        -- </indexedby-7.2>
    })

test:do_execsql_test(
    "indexedby-7.3",
    [[
        EXPLAIN QUERY PLAN DELETE FROM t1 INDEXED BY 'i1' WHERE a = 5
    ]], {
        -- <indexedby-7.3>
        0, 0, 0, "SEARCH TABLE t1 USING COVERING INDEX i1 (a=?)"
        -- </indexedby-7.3>
    })

test:do_execsql_test(
    "indexedby-7.4",
    [[
        EXPLAIN QUERY PLAN DELETE FROM t1 INDEXED BY 'i1' WHERE a = 5 AND b = 10
    ]], {
        -- <indexedby-7.4>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a=?)"
        -- </indexedby-7.4>
    })

test:do_execsql_test(
    "indexedby-7.5",
    [[
        EXPLAIN QUERY PLAN DELETE FROM t1 INDEXED BY 'i2' WHERE a = 5 AND b = 10
    ]], {
        -- <indexedby-7.5>
        0, 0, 0, "SEARCH TABLE t1 USING INDEX i2 (b=?)"
        -- </indexedby-7.5>
    })

-- MUST_WORK_TEST
if (0 > 0) then
    test:do_catchsql_test(
        "indexedby-7.6",
        [[
            DELETE FROM t1 INDEXED BY 'i2' WHERE a = 5
        ]], {
            -- <indexedby-7.6>
            1, "no query solution"
            -- </indexedby-7.6>
        })

    -- Test that "INDEXED BY" can be used in an UPDATE statement.
    -- 
    test:do_execsql_test(
        "indexedby-8.1",
        [[
            EXPLAIN QUERY PLAN UPDATE t1 SET rowid=rowid+1 WHERE a = 5 
        ]], {
            -- <indexedby-8.1>
            0, 0, 0, "SEARCH TABLE t1 USING COVERING INDEX i1 (a=?)"
            -- </indexedby-8.1>
        })

    test:do_execsql_test(
        "indexedby-8.2",
        [[
            EXPLAIN QUERY PLAN UPDATE t1 NOT INDEXED SET rowid=rowid+1 WHERE a = 5 
        ]], {
            -- <indexedby-8.2>
            0, 0, 0, "SCAN TABLE t1"
            -- </indexedby-8.2>
        })

    test:do_execsql_test(
        "indexedby-8.3",
        [[
            EXPLAIN QUERY PLAN UPDATE t1 INDEXED BY i1 SET rowid=rowid+1 WHERE a = 5 
        ]], {
            -- <indexedby-8.3>
            0, 0, 0, "SEARCH TABLE t1 USING COVERING INDEX i1 (a=?)"
            -- </indexedby-8.3>
        })

    test:do_execsql_test(
        "indexedby-8.4",
        [[
            EXPLAIN QUERY PLAN 
            UPDATE t1 INDEXED BY i1 SET rowid=rowid+1 WHERE a = 5 AND b = 10
        ]], {
            -- <indexedby-8.4>
            0, 0, 0, "SEARCH TABLE t1 USING INDEX i1 (a=?)"
            -- </indexedby-8.4>
        })

    test:do_execsql_test(
        "indexedby-8.5",
        [[
            EXPLAIN QUERY PLAN 
            UPDATE t1 INDEXED BY i2 SET rowid=rowid+1 WHERE a = 5 AND b = 10
        ]], {
            -- <indexedby-8.5>
            0, 0, 0, "SEARCH TABLE t1 USING INDEX i2 (b=?)"
            -- </indexedby-8.5>
        })

    test:do_catchsql_test(
        "indexedby-8.6",
        [[
            UPDATE t1 INDEXED BY i2 SET rowid=rowid+1 WHERE a = 5
        ]], {
            -- <indexedby-8.6>
            1, "no query solution"
            -- </indexedby-8.6>
        })

    -- Test that bug #3560 is fixed
end
test:do_execsql_test(
    "indexedby-9.1",
    [[
        CREATE TABLE maintable( id integer PRIMARY KEY );
        CREATE TABLE joinme(id_int integer PRIMARY KEY, id_text text);
        CREATE INDEX joinme_id_text_idx on joinme(id_text);
        CREATE INDEX joinme_id_int_idx on joinme(id_int);
    ]], {
        -- <indexedby-9.1>
        
        -- </indexedby-9.1>
    })

-- MUST_WORK_TEST
if (0 > 0)
 then
    test:do_catchsql_test(
        "indexedby-9.2",
        [[
            select * from maintable as m inner join
            joinme as j indexed by 'joinme_id_text_idx'
            on ( m.id  = j.id_int)
        ]], {
            -- <indexedby-9.2>
            1, "no query solution"
            -- </indexedby-9.2>
        })

    test:do_catchsql_test(
        "indexedby-9.3",
        [[
            select * from maintable, joinme INDEXED by 'joinme_id_text_idx' 
        ]], {
            -- <indexedby-9.3>
            1, "no query solution"
            -- </indexedby-9.3>
        })

    -- Make sure we can still create tables, indices, and columns whose name
    -- is indexed
end
test:do_execsql_test(
    "indexedby-10.1",
    [[
        CREATE TABLE indexed(x PRIMARY KEY,y);
        INSERT INTO indexed VALUES(1,2);
        SELECT * FROM indexed;
    ]], {
        -- <indexedby-10.1>
        1, 2
        -- </indexedby-10.1>
    })

test:do_execsql_test(
    "indexedby-10.2",
    [[
        CREATE INDEX i10 ON indexed(x);
        SELECT * FROM indexed indexed by 'i10' where x>0;
    ]], {
        -- <indexedby-10.2>
        1, 2
        -- </indexedby-10.2>
    })

test:do_execsql_test(
    "indexedby-10.3",
    [[
        DROP TABLE indexed;
        CREATE TABLE t10(indexed INTEGER PRIMARY KEY);
        INSERT INTO t10 VALUES(1);
        CREATE INDEX indexed ON t10(indexed);
        SELECT * FROM t10 indexed by 'indexed' WHERE indexed>0
    ]], {
        -- <indexedby-10.3>
        1
        -- </indexedby-10.3>
    })

-- #-------------------------------------------------------------------------
-- # Ensure that the rowid at the end of each index entry may be used
-- # for equality constraints in the same way as other indexed fields.
-- #
-- do_execsql_test 11.1 {
--   CREATE TABLE x1(a, b TEXT);
--   CREATE INDEX x1i ON x1(a, b);
--   INSERT INTO x1 VALUES(1, 1);
--   INSERT INTO x1 VALUES(1, 1);
--   INSERT INTO x1 VALUES(1, 1);
--   INSERT INTO x1 VALUES(1, 1);
-- }
-- do_execsql_test 11.2 {
--   SELECT a,b,rowid FROM x1 INDEXED BY x1i WHERE a=1 AND b=1 AND rowid=3;
-- } {1 1 3}
-- do_execsql_test 11.3 {
--   SELECT a,b,rowid FROM x1 INDEXED BY x1i WHERE a=1 AND b=1 AND rowid='3';
-- } {1 1 3}
-- do_execsql_test 11.4 {
--   SELECT a,b,rowid FROM x1 INDEXED BY x1i WHERE a=1 AND b=1 AND rowid='3.0';
-- } {1 1 3}
-- do_eqp_test 11.5 {
--   SELECT a,b,rowid FROM x1 INDEXED BY x1i WHERE a=1 AND b=1 AND rowid='3.0';
-- } {0 0 0 {SEARCH TABLE x1 USING COVERING INDEX x1i (a=? AND b=? AND rowid=?)}}
-- do_execsql_test 11.6 {
--   CREATE TABLE x2(c INTEGER PRIMARY KEY, a, b TEXT);
--   CREATE INDEX x2i ON x2(a, b);
--   INSERT INTO x2 VALUES(1, 1, 1);
--   INSERT INTO x2 VALUES(2, 1, 1);
--   INSERT INTO x2 VALUES(3, 1, 1);
--   INSERT INTO x2 VALUES(4, 1, 1);
-- }
-- do_execsql_test 11.7 {
--   SELECT a,b,c FROM x2 INDEXED BY x2i WHERE a=1 AND b=1 AND c=3;
-- } {1 1 3}
-- do_execsql_test 11.8 {
--   SELECT a,b,c FROM x2 INDEXED BY x2i WHERE a=1 AND b=1 AND c='3';
-- } {1 1 3}
-- do_execsql_test 11.9 {
--   SELECT a,b,c FROM x2 INDEXED BY x2i WHERE a=1 AND b=1 AND c='3.0';
-- } {1 1 3}
-- do_eqp_test 11.10 {
--   SELECT a,b,c FROM x2 INDEXED BY x2i WHERE a=1 AND b=1 AND c='3.0';
-- } {0 0 0 {SEARCH TABLE x2 USING COVERING INDEX x2i (a=? AND b=? AND rowid=?)}}


test:finish_test()
